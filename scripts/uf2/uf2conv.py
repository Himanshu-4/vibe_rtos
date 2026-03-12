#!/usr/bin/env python3
"""
uf2conv.py — Convert a raw binary file to UF2 (USB Flashing Format).

UF2 is the drag-and-drop firmware format used by the RP2040 / RP2350
BOOTSEL bootloader and compatible with any UF2-capable MCU.

Reference: https://github.com/microsoft/uf2

Block layout (512 bytes each):
  [  0] magic0      0x0A324655  "UF2\n"
  [  4] magic1      0x9E5D5157
  [  8] flags       0x00002000  family-id present
  [ 12] addr        target flash address for this block
  [ 16] payload_sz  256 (bytes of data in this block)
  [ 20] block_no    0-based block number
  [ 24] num_blocks  total blocks in file
  [ 28] family_id   board-specific identifier
  [ 32] data        256 bytes of firmware, then 220 zero pad bytes
  [508] magic_end   0x0AB16F30

Usage:
  python3 uf2conv.py --bin firmware.bin --out firmware.uf2 \\
                     --family 0xe48bff56 --base 0x10000000

Family IDs (known):
  RP2040         0xe48bff56   (Cortex-M0+)
  RP2350-ARM     0xe48bff59   (Cortex-M33)
  RP2350-RISCV   0xe48bff5b
  SAMD21         0x68ed2b88
  SAMD51         0x55114460
  STM32F1        0x5ee21072
  STM32F4        0x57755a57
"""

import argparse
import struct
import sys
import os

# UF2 constants
UF2_MAGIC0   = 0x0A324655
UF2_MAGIC1   = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_PAYLOAD_SZ = 256
UF2_BLOCK_SZ   = 512
UF2_DATA_FIELD  = 476   # bytes reserved for data in each block


def bin_to_uf2(data: bytes, base_addr: int, family_id: int) -> bytes:
    """Convert raw firmware bytes to a UF2 byte string."""
    # Pad to a multiple of UF2_PAYLOAD_SZ
    remainder = len(data) % UF2_PAYLOAD_SZ
    if remainder:
        data += b'\x00' * (UF2_PAYLOAD_SZ - remainder)

    num_blocks = len(data) // UF2_PAYLOAD_SZ
    blocks = bytearray()

    for block_no in range(num_blocks):
        addr    = base_addr + block_no * UF2_PAYLOAD_SZ
        payload = data[block_no * UF2_PAYLOAD_SZ:(block_no + 1) * UF2_PAYLOAD_SZ]

        # Header (32 bytes)
        header = struct.pack('<IIIIIIII',
            UF2_MAGIC0,
            UF2_MAGIC1,
            UF2_FLAG_FAMILY_ID_PRESENT,
            addr,
            UF2_PAYLOAD_SZ,
            block_no,
            num_blocks,
            family_id,
        )
        # Data field: 256 bytes payload + 220 bytes padding = 476 bytes
        data_field = payload + b'\x00' * (UF2_DATA_FIELD - UF2_PAYLOAD_SZ)
        # Footer (4 bytes)
        footer = struct.pack('<I', UF2_MAGIC_END)

        block = header + data_field + footer
        assert len(block) == UF2_BLOCK_SZ, f"Block size mismatch: {len(block)}"
        blocks.extend(block)

    return bytes(blocks)


def auto_family(family_str: str) -> int:
    """Parse a family ID string (hex or decimal)."""
    return int(family_str, 0)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert a flat binary to UF2 format for drag-and-drop flashing."
    )
    parser.add_argument("--bin",    required=True, metavar="FILE",
                        help="Input binary file (.bin)")
    parser.add_argument("--out",    required=True, metavar="FILE",
                        help="Output UF2 file")
    parser.add_argument("--family", required=True, metavar="HEX",
                        help="UF2 family ID (e.g. 0xe48bff56 for RP2040)")
    parser.add_argument("--base",   default="0x10000000", metavar="HEX",
                        help="Flash base address (default: 0x10000000)")
    args = parser.parse_args()

    bin_path = args.bin
    out_path = args.out
    family_id = auto_family(args.family)
    base_addr = int(args.base, 0)

    if not os.path.isfile(bin_path):
        print(f"ERROR: input file not found: {bin_path}", file=sys.stderr)
        return 1

    with open(bin_path, "rb") as f:
        firmware = f.read()

    if not firmware:
        print(f"ERROR: input file is empty: {bin_path}", file=sys.stderr)
        return 1

    uf2_data = bin_to_uf2(firmware, base_addr, family_id)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(uf2_data)

    num_blocks = len(uf2_data) // UF2_BLOCK_SZ
    print(f"[uf2] {os.path.basename(bin_path)} ({len(firmware):,} bytes) "
          f"→ {os.path.basename(out_path)} "
          f"({num_blocks} blocks, family=0x{family_id:08x}, base=0x{base_addr:08x})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
