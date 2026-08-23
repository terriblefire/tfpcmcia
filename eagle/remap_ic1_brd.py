#!/usr/bin/env python3
"""One-shot: move IC1's <contactref> entries in tfpcmcia.brd to match the
schematic's CH32V467VET6 pin map, so forward/back annotation stays consistent.

Pads do not move — the package is unchanged — so this is purely a permutation of
contactrefs between <signal> blocks, mirroring what remap_ic1.py did to the
schematic's <segment> blocks. Extract-all-then-reinsert, because source and
destination sets overlap.

Routed copper is deliberately left untouched: traces that ran to a pad whose net
changed are now stale and must be ripped up in Eagle before re-routing.
"""
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
SCH = HERE / "tfpcmcia.sch"
BRD = HERE / "tfpcmcia.brd"
LBR = HERE / "SamacSys_Parts.lbr"
PART = "IC1"
DEVICESET = "CH32V467VET6"

_ns = {"__file__": str(HERE / "check_pinmap.py")}
exec((HERE / "check_pinmap.py").read_text().split("def main()")[0], _ns)
TARGET, NC = _ns["TARGET"], _ns["NC"]


def signal_blocks(brd):
    """[(name, body_start, body_end)] for every <signal> block."""
    return [(m.group(1), m.start(2), m.end(2)) for m in
            re.finditer(r'<signal name="([^"]+)">(.*?)</signal>', brd, re.DOTALL)]


def main():
    brd = BRD.read_text(encoding="utf-8")

    # pad map comes from the library's <connects>, not from assumptions
    ds = re.search(rf'<deviceset name="{re.escape(DEVICESET)}".*?</deviceset>',
                   LBR.read_text(encoding="utf-8"), re.DOTALL).group(0)
    pad_of = {m.group(1): int(m.group(2)) for m in
              re.finditer(r'<connect gate="G\$1" pin="([^"]+)" pad="(\d+)"/>', ds)}

    # pad -> target signal name
    want_sig = {}
    for pin, net in TARGET.items():
        if pin not in pad_of:
            sys.exit(f"pin {pin!r} not in {DEVICESET} pad map")
        want_sig[pad_of[pin]] = net
    nc_pads = {pad_of[p] for p in NC if p in pad_of}

    # --- requery current state ------------------------------------------
    # Match at most one trailing newline. Using \s* here erodes surrounding
    # whitespace a little on every run, so the file never settles byte-wise.
    cref = re.compile(rf'<contactref element="{re.escape(PART)}" pad="(\d+)"/>\n?')
    current = {}
    for name, a, b in signal_blocks(brd):
        for m in cref.finditer(brd[a:b]):
            current[int(m.group(1))] = name
    print(f"found {len(current)} existing {PART} contactrefs")

    # --- phase 1: extract every IC1 contactref ---------------------------
    spans = [(m.start(), m.end()) for m in cref.finditer(brd)]
    for s, e in reversed(spans):
        brd = brd[:s] + brd[e:]

    # --- phase 2: re-insert into the target signal -----------------------
    moved = same = added = 0
    created = []
    for pad in sorted(want_sig):
        target = want_sig[pad]
        now = current.get(pad)
        if now is None:
            added += 1
        elif now == target:
            same += 1
        else:
            moved += 1
        entry = f'<contactref element="{PART}" pad="{pad}"/>\n'
        blocks = {n: (a, b) for n, a, b in signal_blocks(brd)}
        if target in blocks:
            a, _ = blocks[target]
            # Insert after the newline that follows the opening tag, so a
            # re-run reproduces byte-identical formatting.
            if brd.startswith("\n", a):
                a += 1
            brd = brd[:a] + entry + brd[a:]
        else:
            close = brd.rfind("</signals>")
            if close < 0:
                sys.exit("no </signals> found")
            brd = (brd[:close]
                   + f'<signal name="{target}">\n{entry}</signal>\n'
                   + brd[close:])
            created.append(target)

    dropped = sorted(p for p in nc_pads if p in current)

    BRD.write_text(brd, encoding="utf-8")

    print(f"re-inserted {len(want_sig)} contactrefs: "
          f"{moved} moved, {same} already correct, {added} newly added")
    if created:
        print(f"created signals: {', '.join(created)}")
    print(f"removed {len(dropped)} contactrefs for NC pads: "
          + ", ".join(f"{p} (was {current[p]})" for p in dropped))

    # --- report signals now carrying no contactref at all ----------------
    dangling = []
    for name, a, b in signal_blocks(brd):
        body = brd[a:b]
        n = body.count("<contactref")
        if n < 2:
            dangling.append((name, n, body.count("<wire")))
    if dangling:
        print("\nsignals with fewer than 2 contactrefs (Eagle will warn):")
        for n, c, w in dangling:
            print(f"  {n:<14} contactrefs={c}  routed wires={w}")


if __name__ == "__main__":
    main()
