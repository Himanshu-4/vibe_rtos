#!/usr/bin/env python3
"""
features_gen.py — VibeRTOS feature macro analyzer and evaluator

Parses C header files starting from an input file, follows conditional
includes based on active -D defines, extracts all macros into typed
dataclass entries, evaluates cross-macro expressions, and writes a
resolved output header (and optionally JSON).

Usage:
  features_gen.py --input test/base.h --env TECH_BT=1 --env TECH_BLE=1 \\
                  --path test/ --output features_generated.h --verbose
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Optional


# ─────────────────────────────────────────────────────────────────────────────
# ANSI color helpers
# ─────────────────────────────────────────────────────────────────────────────
#MARK: logger

class _C:
    RESET   = "\033[0m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    RED     = "\033[91m"
    YELLOW  = "\033[93m"
    GREEN   = "\033[92m"
    CYAN    = "\033[96m"
    BLUE    = "\033[94m"
    MAGENTA = "\033[95m"

    @staticmethod
    def apply(code: str, text: str) -> str:
        return f"{code}{text}{_C.RESET}"

def _red(s: str)     -> str: return _C.apply(_C.RED,     s)
def _yellow(s: str)  -> str: return _C.apply(_C.YELLOW,  s)
def _green(s: str)   -> str: return _C.apply(_C.GREEN,   s)
def _cyan(s: str)    -> str: return _C.apply(_C.CYAN,    s)
def _blue(s: str)    -> str: return _C.apply(_C.BLUE,    s)
def _magenta(s: str) -> str: return _C.apply(_C.MAGENTA, s)
def _dim(s: str)     -> str: return _C.apply(_C.DIM,     s)
def _bold(s: str)    -> str: return _C.apply(_C.BOLD,    s)


# ─────────────────────────────────────────────────────────────────────────────
# Logger
# ─────────────────────────────────────────────────────────────────────────────

class Logger:
    def __init__(self, verbose: bool = False) -> None:
        self.verbose = verbose
        self._counts: dict[str, int] = {}

    def _emit(self, tag: str, msg: str) -> None:
        self._counts[tag] = self._counts.get(tag, 0) + 1
        print(f"{tag}  {msg}", flush=True)

    def info(self, msg: str) -> None:
        self._emit(f"{_green('[INFO]')}", msg)

    def warn(self, msg: str) -> None:
        self._emit(f"{_yellow('[WARN]')}", msg)

    def error(self, msg: str) -> None:
        self._emit(f"{_red('[ERROR]')}", msg)

    def include(self, path: str, depth: int = 0) -> None:
        indent = "  " * depth
        self._emit(f"{_blue('[INCL]')}", f"{indent}↳ {_blue(path)}")

    def skip(self, path: str, reason: str) -> None:
        if self.verbose:
            self._emit(f"{_dim('[SKIP]')}", f"{_dim(path)}  ({reason})")

    def define(self, name: str, value: str, macro_type: str, scope: str) -> None:
        if self.verbose:
            self._emit(
                f"{_cyan('[DEF]')} ",
                f"{_bold(name)} = {_dim(repr(value))}  "
                f"[{_magenta(macro_type)} / {_cyan(scope)}]",
            )

    def eval_result(self, name: str, expr: str, result: Any) -> None:
        if self.verbose:
            self._emit(
                f"{_magenta('[EVAL]')}",
                f"{_bold(name):40s} {_dim(expr)} {_cyan('→')} {_cyan(str(result))}",
            )

    def debug(self, msg: str) -> None:
        if self.verbose:
            self._emit(f"{_dim('[DBG]')} ", _dim(msg))

    def summary(self) -> None:
        warns  = self._counts.get("[WARN]", 0)
        errors = self._counts.get("[ERROR]", 0)
        if errors:
            self.error(f"Finished with {errors} error(s), {warns} warning(s)")
        elif warns:
            self.warn(f"Finished with {warns} warning(s)")


# ─────────────────────────────────────────────────────────────────────────────
# Data model
# ─────────────────────────────────────────────────────────────────────────────
#MARK: data model
class MacroType(str, Enum):
    ENV        = "env"         # came from -D / --env
    RAW        = "raw"         # literal: #define FOO 42
    EXPRESSION = "expression"  # references other macros / arithmetic
    GUARDED    = "guarded"     # defined inside a #if / #elif / #else block


class MacroScope(str, Enum):
    ENV      = "env"       # command-line define
    PLATFORM = "platform"  # sdkconfig / hardware (CONFIG_* names)
    LOCAL    = "local"     # feature header


@dataclass
class MacroEntry:
    name:            str
    raw_value:       str
    type:            MacroType
    scope:           MacroScope
    source_file:     str
    line_no:         int
    guard_condition: Optional[str] = None   # innermost #if condition string
    evaluated_value: Any           = None   # filled by Evaluator
    comment:         str           = ""     # human-readable: "expr → result"


# ─────────────────────────────────────────────────────────────────────────────
# Condition stack  (handles #if / #ifdef / #ifndef / #elif / #else / #endif)
# ─────────────────────────────────────────────────────────────────────────────
#MARK: condition stack

# Translate C preprocessor operators to Python equivalents before eval()
_C_TO_PY = [
    (r"&&",           " and "),
    (r"\|\|",         " or  "),
    (r"!\s*(?=[^=])", " not "),
]

def _c_expr_to_py(expr: str) -> str:
    for pattern, repl in _C_TO_PY:
        expr = re.sub(pattern, repl, expr)
    return expr


# Matches the innermost ternary where none of condition/true/false contain
# parens or the ternary operators themselves.  Works innermost-first so
# nested ternaries are resolved in multiple passes.
_TERNARY_INNER = re.compile(r'\(([^()?:]+)\?\s*([^()?:]+)\s*:\s*([^()?:]+)\)')


def _eval_ternaries(expr: str) -> str:
    """Evaluate C ternary operators innermost-first, iteratively.

    Handles both literal conditions (1 ? 32 : 0) and expression conditions
    (5 >= 5 ? 0xFF : 0x0F) by using Python eval on the condition part.
    Nested ternaries are resolved via repeated passes.
    """
    prev = None
    while expr != prev:
        prev = expr

        def _repl(m: re.Match) -> str:
            cond_s = m.group(1).strip()
            t, f   = m.group(2).strip(), m.group(3).strip()
            try:
                cond_val = eval(cond_s, {"__builtins__": {}}, {})  # noqa: S307
                return t if cond_val else f
            except Exception:
                return m.group(0)  # cannot evaluate yet, leave unchanged

        expr = _TERNARY_INNER.sub(_repl, expr)
    return expr


class ConditionStack:
    """Best-effort #if evaluation against a live symbol dictionary."""

    def __init__(self, symbols: dict[str, Any]) -> None:
        # Reference to the shared symbol table so it stays current.
        self.symbols = symbols
        self._stack: list[dict] = []

    # ── public state ──────────────────────────────────────────────────────────

    @property
    def active(self) -> bool:
        return all(frame["taken"] for frame in self._stack)

    @property
    def current_condition(self) -> Optional[str]:
        """Innermost non-include-guard condition, or None."""
        for frame in reversed(self._stack):
            if frame.get("is_guard"):
                continue
            if frame["condition"] not in (None, "else"):
                return frame["condition"]
        return None

    # ── directives ────────────────────────────────────────────────────────────

    def push_ifdef(self, name: str) -> None:
        taken = name in self.symbols
        cond  = f"defined({name})"
        self._stack.append({"condition": cond, "taken": taken, "had_true": taken,
                             "is_guard": False})

    def push_ifndef(self, name: str) -> None:
        taken = name not in self.symbols
        cond  = f"!defined({name})"
        # Detect file-level include guards: _FOO_H or _FOO_H_
        is_guard = bool(re.match(r'^_[A-Z0-9_]+H_?$', name))
        self._stack.append({"condition": cond, "taken": taken, "had_true": taken,
                             "is_guard": is_guard})

    def push_if(self, condition: str) -> None:
        taken = self._eval(condition)
        self._stack.append({"condition": condition, "taken": taken, "had_true": taken,
                             "is_guard": False})

    def handle_elif(self, condition: str) -> None:
        if not self._stack:
            return
        frame = self._stack[-1]
        taken = (not frame["had_true"]) and self._eval(condition)
        frame["condition"] = condition
        frame["taken"]     = taken
        frame["had_true"]  = frame["had_true"] or taken

    def handle_else(self) -> None:
        if not self._stack:
            return
        frame = self._stack[-1]
        frame["condition"] = "else"
        frame["taken"]     = not frame["had_true"]

    def pop(self) -> None:
        if self._stack:
            self._stack.pop()

    # ── internal ──────────────────────────────────────────────────────────────

    def _eval(self, condition: str) -> bool:
        try:
            expr = condition

            # Replace defined(X) / defined X
            def _replace_defined(m: re.Match) -> str:
                return "1" if m.group(1) in self.symbols else "0"
            expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)", _replace_defined, expr)
            expr = re.sub(r"defined\s+(\w+)",            _replace_defined, expr)

            # Multi-pass substitution: loop until stable (handles macro chains
            # like FULL_FEATURED → HAS_WIRELESS → TECH_BT across multiple levels)
            for _ in range(15):
                prev = expr
                for name in sorted(self.symbols.keys(), key=len, reverse=True):
                    val = self.symbols[name]
                    if isinstance(val, bool):
                        val = int(val)
                    expr = re.sub(rf"\b{re.escape(name)}\b", str(val), expr)
                if expr == prev:
                    break

            # Replace remaining unknown identifiers with 0
            expr = re.sub(r"\b[A-Za-z_]\w*\b", "0", expr)

            # Evaluate numeric ternaries before Python operator translation
            expr = _eval_ternaries(expr)

            # C → Python operators
            expr = _c_expr_to_py(expr)

            result = eval(expr, {"__builtins__": {}}, {})  # noqa: S307
            return bool(result)
        except Exception:
            return False


# ─────────────────────────────────────────────────────────────────────────────
# Classifier helpers
# ─────────────────────────────────────────────────────────────────────────────
#MARK: classifiers helper
_LITERAL_PAT = re.compile(
    r"^(-?\d+|0x[0-9a-fA-F]+|0b[01]+|0[0-7]+|-?\d+\.\d+|\"[^\"]*\"|'[^']*')$"
)
_IDENT_PAT = re.compile(r"\b[A-Za-z_]\w*\b")
_EXPR_OPS  = frozenset(["&&", "||", "!", ">=", "<=", "==", "!=", ">", "<",
                         "+", "-", "*", "/", "?", ":", "<<", ">>", "~", "^"])


def _classify_type(value: str, guard: Optional[str]) -> MacroType:
    if guard is not None:
        return MacroType.GUARDED
    if not value:
        return MacroType.RAW
    stripped = value.strip()
    if _LITERAL_PAT.match(stripped):
        return MacroType.RAW
    if any(op in stripped for op in _EXPR_OPS):
        return MacroType.EXPRESSION
    if _IDENT_PAT.search(stripped):
        return MacroType.EXPRESSION
    return MacroType.RAW


def _classify_scope(source_file: str, name: str) -> MacroScope:
    basename = os.path.basename(source_file).lower()
    if basename == "sdkconfig.h" or name.startswith("CONFIG_"):
        return MacroScope.PLATFORM
    return MacroScope.LOCAL


# ─────────────────────────────────────────────────────────────────────────────
# Header parser
# ─────────────────────────────────────────────────────────────────────────────
#MARK: Header parser
# Pre-compiled directive regexes
_RE_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
_RE_DEFINE  = re.compile(r'^\s*#\s*define\s+(\w+)(?:\s+(.*?))?$')
_RE_IFDEF   = re.compile(r'^\s*#\s*ifdef\s+(\w+)')
_RE_IFNDEF  = re.compile(r'^\s*#\s*ifndef\s+(\w+)')
_RE_IF      = re.compile(r'^\s*#\s*if\b(.*)')
_RE_ELIF    = re.compile(r'^\s*#\s*elif\b(.*)')
_RE_ELSE    = re.compile(r'^\s*#\s*else\b')
_RE_ENDIF   = re.compile(r'^\s*#\s*endif\b')

# Include-guard patterns to skip (e.g. _FOO_H, _FOO_H_)
_GUARD_PAT  = re.compile(r'^_[A-Z0-9_]+H_?$')


class HeaderParser:
    def __init__(
        self,
        include_paths: list[Path],
        env: dict[str, Any],
        log: Logger,
    ) -> None:
        self.include_paths = include_paths
        self.log           = log
        self.macros:   list[MacroEntry] = []
        self._visited: set[str]         = set()

        # Shared live symbol table: env defines + macros as they are parsed.
        # The ConditionStack gets a reference to this dict so it always sees
        # the latest values.
        self._symbols: dict[str, Any] = dict(env)

    # ── public entry point ────────────────────────────────────────────────────

    def parse_file(self, path: Path, depth: int = 0) -> None:
        resolved = self._resolve(path)
        if resolved is None:
            self.log.warn(f"Cannot resolve include: {path}")
            return

        key = str(resolved.resolve())
        if key in self._visited:
            self.log.skip(resolved.name, "already parsed")
            return
        self._visited.add(key)
        self.log.include(str(resolved), depth)

        lines = self._read_lines(resolved)
        cond  = ConditionStack(self._symbols)

        for lineno, line in enumerate(lines, start=1):
            stripped = line.strip()

            # Order matters: endif/else/elif must be checked before ifdef/if
            if _RE_ENDIF.match(stripped):
                cond.pop()
            elif m := _RE_ELSE.match(stripped):
                cond.handle_else()
            elif m := _RE_ELIF.match(stripped):
                cond.handle_elif(m.group(1).strip())
            elif m := _RE_IFDEF.match(stripped):
                cond.push_ifdef(m.group(1))
            elif m := _RE_IFNDEF.match(stripped):
                cond.push_ifndef(m.group(1))
            elif m := _RE_IF.match(stripped):
                cond.push_if(m.group(1).strip())
            elif m := _RE_INCLUDE.match(stripped):
                if cond.active:
                    self.parse_file(Path(m.group(1)), depth + 1)
            elif m := _RE_DEFINE.match(stripped):
                if cond.active:
                    self._record_define(
                        name=m.group(1),
                        value=(m.group(2) or "").strip(),
                        source=str(resolved),
                        lineno=lineno,
                        guard=cond.current_condition,
                    )

    # ── internals ─────────────────────────────────────────────────────────────

    def _record_define(
        self,
        name: str,
        value: str,
        source: str,
        lineno: int,
        guard: Optional[str],
    ) -> None:
        # Skip include guards
        if not value and _GUARD_PAT.match(name):
            return

        macro_type = _classify_type(value, guard)
        scope      = _classify_scope(source, name)

        entry = MacroEntry(
            name=name,
            raw_value=value,
            type=macro_type,
            scope=scope,
            source_file=source,
            line_no=lineno,
            guard_condition=guard,
        )
        self.macros.append(entry)

        # Always update live symbol table — last definition wins, matching
        # C preprocessor semantics.  Warn when a macro is redefined.
        new_val = value if value else 1
        if name in self._symbols and str(self._symbols[name]) != str(new_val):
            self.log.warn(
                f"Redefine: {_bold(name)}  "
                f"{_dim(repr(str(self._symbols[name])))} → "
                f"{_cyan(repr(str(new_val)))}  "
                f"({os.path.basename(source)}:{lineno})"
            )
        self._symbols[name] = new_val

        self.log.define(name, value, macro_type.value, scope.value)

    def _resolve(self, path: Path) -> Optional[Path]:
        if path.is_absolute() and path.exists():
            return path
        for base in self.include_paths:
            candidate = base / path
            if candidate.exists():
                return candidate
        return None

    @staticmethod
    def _read_lines(path: Path) -> list[str]:
        """Read file and join line-continuation backslashes."""
        raw = path.read_text(errors="replace")
        # Join continued lines so #define foo \n    bar → single logical line
        raw = re.sub(r"\\\n", " ", raw)
        return raw.splitlines()


# ─────────────────────────────────────────────────────────────────────────────
# Evaluator
# ─────────────────────────────────────────────────────────────────────────────
#MARK: Evaluator
class Evaluator:
    """
    Iteratively evaluates all MacroEntry values, substituting known symbols
    into expressions until stable (handles cross-file dependencies).
    """

    def __init__(self, env: dict[str, Any], log: Logger) -> None:
        self.log = log
        # Start with env macros as integers/strings
        self.symbols: dict[str, Any] = {
            k: (v if not isinstance(v, str) else self._parse_literal(v) or v)
            for k, v in env.items()
        }

    def evaluate_all(self, macros: list[MacroEntry]) -> None:
        # Pass 1: env entries (already have evaluated_value)
        for m in macros:
            if m.type == MacroType.ENV:
                m.evaluated_value = self.symbols.get(m.name, self._parse_literal(m.raw_value))
                m.comment = f"-D{m.name}"
                self.symbols[m.name] = m.evaluated_value

        # Pass 2: RAW literals (no dependencies)
        for m in macros:
            if m.type == MacroType.RAW:
                self._eval_entry(m)

        # Pass 3: expressions + guarded — iterate until stable
        changed = True
        max_iters = 12
        while changed and max_iters > 0:
            changed = False
            max_iters -= 1
            for m in macros:
                if m.type in (MacroType.EXPRESSION, MacroType.GUARDED):
                    prev = m.evaluated_value
                    self._eval_entry(m)
                    if m.evaluated_value != prev:
                        changed = True

        # Pass 4: anything still None
        for m in macros:
            if m.evaluated_value is None:
                self._eval_entry(m)

    # ── per-entry eval ────────────────────────────────────────────────────────

    def _eval_entry(self, m: MacroEntry) -> None:
        if not m.raw_value:
            # Bare flag: #define FEATURE_X  (no value)
            m.evaluated_value = 1
            m.comment = "(flag — defined)"
            self.symbols.setdefault(m.name, 1)
            return

        val, comment = self._eval_expr(m.raw_value)
        m.evaluated_value = val
        m.comment = comment
        if val is not None:
            self.symbols[m.name] = val
            self.log.eval_result(m.name, m.raw_value, val)

    def _eval_expr(self, expr: str) -> tuple[Any, str]:
        """Return (evaluated_value, comment_string)."""
        if not expr:
            return 1, "(flag)"

        # Pure literal?
        lit = self._parse_literal(expr)
        if lit is not None:
            return lit, str(lit)

        # Multi-pass substitution: loop until stable (handles macro chains)
        resolved = expr
        for _ in range(15):
            prev_r = resolved
            for name in sorted(self.symbols.keys(), key=len, reverse=True):
                val = self.symbols[name]
                if isinstance(val, bool):
                    val = int(val)
                elif isinstance(val, str):
                    coerced = self._parse_literal(val)
                    val = coerced if coerced is not None else val
                resolved = re.sub(rf"\b{re.escape(name)}\b", str(val), resolved)
            if resolved == prev_r:
                break

        # Evaluate numeric ternaries (e.g. (1 ? 32 : 0) → 32)
        resolved = _eval_ternaries(resolved)

        # Translate C operators to Python
        py_expr = _c_expr_to_py(resolved)

        try:
            result  = eval(py_expr, {"__builtins__": {}}, {})  # noqa: S307
            display = int(result) if isinstance(result, bool) else result
            comment = f"{expr} → {display}"
            return result, comment
        except Exception:
            pass

        # Partial fallback: replace remaining unknown identifiers with 0 on
        # the C-form resolved string (before Python translation), so that
        # Python keywords like `or`/`and` introduced by _c_expr_to_py are
        # not accidentally zeroed out.
        partial_c = re.sub(r"\b[A-Za-z_]\w*\b", "0", resolved)
        partial_c = _eval_ternaries(partial_c)
        partial_py = _c_expr_to_py(partial_c)
        try:
            result  = eval(partial_py, {"__builtins__": {}}, {})  # noqa: S307
            display = int(result) if isinstance(result, bool) else result
            comment = f"{expr} ~→ {display} (partial)"
            return result, comment
        except Exception:
            return None, f"{expr} (unresolved)"

    @staticmethod
    def _parse_literal(s: str) -> Optional[Any]:
        s = s.strip()
        if not s:
            return None
        try:
            if re.match(r'^0[xX][0-9a-fA-F]+$', s):
                return int(s, 16)
            if re.match(r'^0[bB][01]+$', s):
                return int(s, 2)
            if re.match(r'^0[0-7]+$', s) and len(s) > 1:
                return int(s, 8)
            if re.match(r'^-?\d+$', s):
                return int(s)
            if re.match(r'^-?\d+\.\d+$', s):
                return float(s)
            if s.startswith('"') and s.endswith('"'):
                return s[1:-1]
        except ValueError:
            pass
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Output writers
# ─────────────────────────────────────────────────────────────────────────────
#MARK: output writers
def _val_to_str(val: Any, raw: str) -> str:
    if val is None:
        return raw if raw else "/* undefined */"
    if isinstance(val, bool):
        return "1" if val else "0"
    if isinstance(val, float) and val == int(val):
        return str(int(val))
    if isinstance(val, str):
        return f'"{val}"'
    return str(val)


def write_header(
    macros:    list[MacroEntry],
    env:       dict[str, Any],
    out_path:  Path,
    log:       Logger,
) -> None:
    lines: list[str] = []
    lines += [
        "/*",
        " * Auto-generated by features_gen.py — DO NOT EDIT",
        " */",
        "",
        "#ifndef _FEATURES_GENERATED_H",
        "#define _FEATURES_GENERATED_H",
        "",
    ]

    # ── Environment block ─────────────────────────────────────────────────────
    # Find env macros that were later overridden by a header define — those
    # will appear in the per-file block with the final value, so suppress the
    # #define here and add an override note instead.
    overridden_env = {m.name for m in macros if m.type != MacroType.ENV} & set(env)

    if env:
        lines.append("/* ── Environment / compiler defines " + "─" * 43 + " */")
        for name, val in env.items():
            if name in overridden_env:
                # Find the overriding entry to name the source
                over_m = next((m for m in reversed(macros)
                               if m.name == name and m.type != MacroType.ENV), None)
                src = (f"{os.path.basename(over_m.source_file)}:{over_m.line_no}"
                       if over_m else "header")
                lines.append(
                    f"/* -D{name}={val}  →  overridden to"
                    f" {_val_to_str(over_m.evaluated_value, str(val)) if over_m else '?'}"
                    f" by {src} */"
                )
            else:
                lines.append(f"/* -D{name}={val} */")
                lines.append(f"#define {name:<44} {val}")
        lines.append("")

    # ── Per-file blocks ───────────────────────────────────────────────────────
    by_file: dict[str, list[MacroEntry]] = {}
    for m in macros:
        if m.type != MacroType.ENV:
            by_file.setdefault(m.source_file, []).append(m)

    for filepath, entries in by_file.items():
        fname = os.path.basename(filepath)
        pad   = max(0, 57 - len(fname))
        lines.append(f"/* ── {fname} " + "─" * pad + " */")
        for m in entries:
            guard_note = f"  [if {m.guard_condition}]" if m.guard_condition else ""
            comment    = m.comment or m.raw_value
            lines.append(f"/* {comment}{guard_note} */")
            val_str = _val_to_str(m.evaluated_value, m.raw_value)
            lines.append(f"#define {m.name:<44} {val_str}")
        lines.append("")

    lines += ["#endif /* _FEATURES_GENERATED_H */", ""]

    out_path.write_text("\n".join(lines))
    log.info(
        f"Header written → {_cyan(str(out_path))}"
        f"  ({_cyan(str(len(macros)))} macros)"
    )


def write_json(macros: list[MacroEntry], out_path: Path, log: Logger) -> None:
    data = [
        {
            "name":            m.name,
            "raw_value":       m.raw_value,
            "type":            m.type.value,
            "scope":           m.scope.value,
            "source_file":     os.path.basename(m.source_file),
            "line_no":         m.line_no,
            "guard_condition": m.guard_condition,
            "evaluated_value": m.evaluated_value,
            "comment":         m.comment,
        }
        for m in macros
    ]
    out_path.write_text(json.dumps(data, indent=2, default=str))
    log.info(f"JSON written   → {_cyan(str(out_path))}")


# ─────────────────────────────────────────────────────────────────────────────
# CLI argument parsing
# ─────────────────────────────────────────────────────────────────────────────
#MARK: CLI argument parsing
def _build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="features_gen.py",
        description="VibeRTOS feature macro analyzer and evaluator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  # Feature-flag syntax (INCLUDE_FEATURE=1, NOT_INCLUDE_FEATURE=0)
  %(prog)s -i test/base.h -p test/ \\
      TECH_BT=INCLUDE_FEATURE TECH_BLE=INCLUDE_FEATURE \\
      TECH_AUDIO=NOT_INCLUDE_FEATURE TECH_PARAM=NOT_INCLUDE_FEATURE

  # Mix feature flags with raw values and options
  %(prog)s -i test/base.h -p test/ -o out.h -t rp2040 --json \\
      TECH_BT=INCLUDE_FEATURE TECH_BLE=INCLUDE_FEATURE \\
      TECH_AUDIO=INCLUDE_FEATURE TECH_PARAM=INCLUDE_FEATURE BT_VER=6

  # Classic -D style still works
  %(prog)s -i test/base.h -D TECH_BT=1 -D TECH_BLE=1 -p test/ --verbose

  # BLE only, JSON dump
  %(prog)s -i test/base.h -p test/ --json --target rp2350 \\
      TECH_BLE=INCLUDE_FEATURE
""",
    )
    p.add_argument(
        "--input", "-i", metavar="FILE", required=True,
        help="Top-level header file to parse (e.g. test/base.h)",
    )
    p.add_argument(
        "--env", "-D", metavar="DEF", action="append", default=[],
        help="Compiler define: NAME or NAME=VALUE (repeatable; -DNAME style also accepted)",
    )
    p.add_argument(
        "--path", "-p", metavar="DIR", action="append", default=[],
        help="Header search path (repeatable)",
    )
    p.add_argument(
        "--include-path", metavar="DIR", action="append", default=[],
        dest="include_path",
        help="Additional include search path (same as --path)",
    )
    p.add_argument(
        "--output", "-o", metavar="FILE", default=None,
        help="Output .h file (default: features_generated.h next to input)",
    )
    p.add_argument(
        "--json", "-j", action="store_true",
        help="Also emit a .json dump of all parsed macro entries",
    )
    p.add_argument(
        "--target", "-t", metavar="BOARD", default=None,
        help="Target board name, informational (e.g. rp2040, rp2350)",
    )
    p.add_argument(
        "--verbose", "-v", action="store_true",
        help="Enable verbose/debug output",
    )
    p.add_argument(
        "features", nargs="*",
        metavar="NAME=INCLUDE_FEATURE|NOT_INCLUDE_FEATURE",
        help=(
            "Feature flags: NAME=INCLUDE_FEATURE (→ NAME=1) or "
            "NAME=NOT_INCLUDE_FEATURE (→ NAME=0). "
            "Raw values also accepted: BT_VER=6."
        ),
    )
    return p


def _parse_env_defs(raw: list[str], log: Logger) -> dict[str, Any]:
    """Parse ['-DTECH_BT', 'TECH_BLE=1', 'BT_VER=6'] → {'TECH_BT': 1, ...}
    ['TECH_BT=NOT_INCLUDE_FEATURE', 'TECH_BLE=INCLUDE_FEATURE', 'BT_VER=6'] → {'TECH_BT': 0, TECH_BLE: 1, BT_VER: 6}
    """
    env: dict[str, Any] = {}
    for token in raw:
        # Strip leading -D or -d
        token = re.sub(r"^-[Dd]", "", token).strip()
        if "=" in token:
            name, _, raw_val = token.partition("=")
            if raw_val == "INCLUDE_FEATURE":
                val: Any = 1
            elif raw_val == "NOT_INCLUDE_FEATURE":
                val = 0
            else:
                try:
                    val = int(raw_val, 0)
                except ValueError:
                    val = raw_val
        else:
            name, val = token, 1
        if not name:
            log.warn(f"Ignoring empty define token: {token!r}")
            continue
        env[name] = val
        log.debug(f"env: {name} = {val}")
    return env


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────
#MARK: main
def main() -> int:
    parser = _build_argparser()
    args   = parser.parse_args()
    log    = Logger(verbose=args.verbose)

    # ── Banner ────────────────────────────────────────────────────────────────
    log.info(_bold("VibeRTOS features_gen.py"))
    if args.target:
        log.info(f"Target: {_cyan(args.target)}")

    # ── Env defines + positional feature flags ────────────────────────────────
    # Merge -D / --env tokens with positional NAME=INCLUDE/NOT_INCLUDE tokens.
    all_raw = args.env + (args.features or [])
    env = _parse_env_defs(all_raw, log)

    # Feature-flag summary (INCLUDE_FEATURE / NOT_INCLUDE_FEATURE tokens only)
    included = [t.partition("=")[0] for t in (args.features or [])
                if t.endswith("=INCLUDE_FEATURE")]
    excluded = [t.partition("=")[0] for t in (args.features or [])
                if t.endswith("=NOT_INCLUDE_FEATURE")]
    if included:
        log.info(f"Features ON:  {' '.join(_cyan(n) for n in included)}")
    if excluded:
        log.info(f"Features OFF: {' '.join(_yellow(n) for n in excluded)}")
    if env:
        active = ", ".join(
            f"{_cyan(k)}={_dim(str(v))}" for k, v in env.items()
        )
        log.info(f"Active defines: {active}")

    # ── Resolve input file ────────────────────────────────────────────────────
    input_path = Path(args.input).resolve()
    if not input_path.exists():
        log.error(f"Input file not found: {input_path}")
        return 1

    # ── Build include search paths ────────────────────────────────────────────
    include_paths: list[Path] = [input_path.parent]
    for d in args.path + args.include_path:
        p = Path(d).resolve()
        if not p.is_dir():
            log.warn(f"Include path does not exist: {p}")
        else:
            include_paths.append(p)
    log.debug(f"Include paths: {[str(p) for p in include_paths]}")

    # ── Parse ─────────────────────────────────────────────────────────────────
    hparser = HeaderParser(include_paths=include_paths, env=env, log=log)

    # Seed env entries as synthetic MacroEntry objects
    for name, val in env.items():
        hparser.macros.append(MacroEntry(
            name=name, raw_value=str(val),
            type=MacroType.ENV, scope=MacroScope.ENV,
            source_file="<env>", line_no=0,
            evaluated_value=val, comment=f"-D{name}={val}",
        ))

    hparser.parse_file(input_path)

    all_macros = hparser.macros
    if not all_macros:
        log.warn("No macros found — check --path and --input arguments.")
        return 1

    log.info(
        f"Parsed {_cyan(str(len(all_macros)))} macros "
        f"from {_cyan(str(len(hparser._visited)))} file(s)"
    )

    # ── Evaluate ──────────────────────────────────────────────────────────────
    evaluator = Evaluator(env=env, log=log)
    evaluator.evaluate_all(all_macros)

    unresolved = [m for m in all_macros if m.evaluated_value is None]
    if unresolved:
        log.warn(
            f"{len(unresolved)} macro(s) could not be fully resolved: "
            + ", ".join(_yellow(m.name) for m in unresolved)
        )

    # ── Deduplicate: last definition of each name wins (C preprocessor) ───────
    # Keep all macros for evaluation context but only emit the final definition.
    last_idx: dict[str, int] = {m.name: i for i, m in enumerate(all_macros)}
    output_macros = [m for i, m in enumerate(all_macros) if last_idx[m.name] == i]
    dupes = len(all_macros) - len(output_macros)
    if dupes:
        log.info(f"Deduplicated {_cyan(str(dupes))} redefined macro(s) — keeping last definition")

    # ── Write output ──────────────────────────────────────────────────────────
    out_path = (
        Path(args.output)
        if args.output
        else input_path.parent / "features_generated.h"
    )
    write_header(output_macros, env, out_path, log)

    if args.json:
        write_json(output_macros, out_path.with_suffix(".json"), log)

    log.summary()
    log.info(_green("Done."))
    return 0


if __name__ == "__main__":
    sys.exit(main())
