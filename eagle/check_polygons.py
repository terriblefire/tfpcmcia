#!/usr/bin/env python3
"""Check the board for zero-width copper pour polygons.

A polygon with width 0 produces zero-width draws in the Gerber output,
which the boardhouse rejects or queries, delaying the order. Every pour
must have a real outline width (e.g. 0.254mm).

    python3 check_polygons.py [tfpcmcia.brd]

Exit status is 0 only when no zero-width pour polygons exist.
"""
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
BRD = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "tfpcmcia.brd"

COPPER_LAYERS = {str(n) for n in range(1, 17)}   # Top, Route2..Route15, Bottom


def main():
    root = ET.parse(str(BRD)).getroot()

    bad = []
    for signal in root.iter("signal"):
        for poly in signal.iter("polygon"):
            if poly.get("layer") not in COPPER_LAYERS:
                continue
            if float(poly.get("width", "0")) == 0:
                v = poly.find("vertex")
                at = f" near ({v.get('x')}, {v.get('y')})" if v is not None else ""
                bad.append(f"   signal {signal.get('name'):<10} layer {poly.get('layer'):>2}{at}")

    if bad:
        print(f"FAIL - {len(bad)} zero-width pour polygon(s) in {BRD.name}")
        for line in bad:
            print(line)
        print("\nSet a real width on each polygon in EAGLE (e.g. 0.254mm):")
        print("select the polygon edge, then change its Width property.")
        return 1

    print(f"PASS - no zero-width pour polygons in {BRD.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
