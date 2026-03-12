#!/usr/bin/env python3
"""Peek a single Amiga word address via DiagROM serial console.

Usage:
    python3 diagrom_peek.py [hex_address]

Example:
    python3 diagrom_peek.py 600000   # expect BEEF from PCMCIA common memory
    python3 diagrom_peek.py 600001   # byte access

NOTE: DiagROM command syntax must be confirmed before use.
      Update DIAGROM_CMD below once the exact format is known.
      Connect the Amiga serial port (/dev/tty.usbserial-FTB6SPL3) running DiagROM.
"""

import serial
import sys
import time

PORT = '/dev/tty.usbserial-FTB6SPL3'
BAUD = 9600

# TODO: Replace with confirmed DiagROM command syntax.
# Common possibilities:
#   "m {addr:06X}\r\n"    — memory examine
#   "peek {addr:08X}\r\n" — peek command
# Ask user / check DiagROM source to confirm.
DIAGROM_CMD = "m {addr:06X}\r\n"


def peek(address):
    with serial.Serial(PORT, BAUD, timeout=2) as s:
        time.sleep(0.1)
        s.reset_input_buffer()
        cmd = DIAGROM_CMD.format(addr=address)
        print(f"Sending: {cmd.strip()!r}")
        s.write(cmd.encode())
        time.sleep(0.3)
        response = s.read(s.in_waiting or 256).decode('ascii', errors='replace')
        print(f"Response: {response!r}")
        print(response)


if __name__ == '__main__':
    addr = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x600000
    peek(addr)
