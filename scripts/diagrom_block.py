#!/usr/bin/env python3
"""Read a block of Amiga memory via DiagROM and dump in hex format.

Usage:
    python3 diagrom_block.py [start_hex] [length_decimal]

Example:
    python3 diagrom_block.py 600000 32   # dump 32 bytes from PCMCIA base

NOTE: DiagROM command syntax must be confirmed before use.
      Update DIAGROM_CMD below to match your DiagROM version.
      Output format: 600000: BEEF DEAD CAFE BABE ...
"""

import serial
import sys
import time

PORT = '/dev/tty.usbserial-FTB6SPL3'
BAUD = 9600

# TODO: Replace with confirmed DiagROM command syntax.
DIAGROM_CMD = "m {addr:06X}\r\n"

INTER_CMD_DELAY = 0.15  # seconds between commands


def read_word(s, address):
    """Send a peek command and parse a 16-bit hex value from the response."""
    s.reset_input_buffer()
    cmd = DIAGROM_CMD.format(addr=address)
    s.write(cmd.encode())
    time.sleep(INTER_CMD_DELAY)
    response = s.read(s.in_waiting or 64).decode('ascii', errors='replace')
    # Attempt to extract the last hex word from the response.
    # Adjust parsing once DiagROM response format is known.
    tokens = response.split()
    for token in reversed(tokens):
        try:
            return int(token, 16)
        except ValueError:
            continue
    return None


def dump_block(start, length):
    """Dump 'length' bytes (word-aligned) starting at 'start'."""
    words = (length + 1) // 2
    with serial.Serial(PORT, BAUD, timeout=2) as s:
        time.sleep(0.1)
        addr = start & ~1   # align to word boundary
        for i in range(words):
            val = read_word(s, addr)
            col = i % 8
            if col == 0:
                if i > 0:
                    print()
                print(f"{addr:06X}: ", end='')
            if val is not None:
                print(f"{val:04X} ", end='', flush=True)
            else:
                print("???? ", end='', flush=True)
            addr += 2
        print()


if __name__ == '__main__':
    start  = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x600000
    length = int(sys.argv[2])     if len(sys.argv) > 2 else 32
    dump_block(start, length)
