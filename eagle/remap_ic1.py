#!/usr/bin/env python3
"""One-shot: permute IC1's net assignments for the CH32V467VET6 pinout.

Eagle preserved every net on its original *pad* when the device was swapped, so
the wiring is now attached to the wrong functions. Each IC1 pin owns an isolated
{pinref, wire, label} segment whose geometry already sits at that pin's location,
so the fix is a pure permutation of segments between <net> blocks — no geometry
is moved. The 7 pins that were never wired get new stubs with neighbour-matched
geometry; the 7 that must be NC have their segments dropped.

Uses the extract-all-then-reinsert pattern because source and destination sets
overlap heavily (A16 leaves PB0 for PB12, which currently carries RESET).

Board file is deliberately NOT touched.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path.home() / "Downloads/eagle-edit/scripts"))
import _geom  # noqa: E402

HERE = Path(__file__).resolve().parent
SCH = HERE / "tfpcmcia.sch"
PART = "IC1"
SYMBOL = "CH32V467VET6"

# Target map and NC set live in check_pinmap.py — single source of truth.
_ns = {"__file__": str(HERE / "check_pinmap.py")}
exec((HERE / "check_pinmap.py").read_text().split("def main()")[0], _ns)
TARGET, NC = _ns["TARGET"], _ns["NC"]


def net_blocks(sch):
    """[(name, start_of_body, end_of_body)] for every <net> block."""
    return [(m.group(1), m.start(2), m.end(2)) for m in
            re.finditer(r'<net name="([^"]+)" class="0">(.*?)</net>', sch, re.DOTALL)]


def main():
    sch = SCH.read_text(encoding="utf-8")

    if sch.count("<sheet>") != 1:
        sys.exit("expected a single-sheet schematic")

    # --- requery current state from the file we are about to mutate --------
    sym = _geom.parse_symbol_pins(sch, SYMBOL)
    inst = _geom.find_instance(sch, PART)
    existing = _geom.parse_existing_stubs(sch, PART)

    # Every segment mentioning PART, with its owning net.
    owned = []          # (pin, segment_text, seg_start, seg_end, net_name)
    for nm in re.finditer(r'<net name="([^"]+)" class="0">(.*?)</net>', sch, re.DOTALL):
        base = nm.start(2)
        for sm in re.finditer(r'<segment>(.*?)</segment>\s*', nm.group(2), re.DOTALL):
            body = sm.group(1)
            pr = re.search(rf'<pinref part="{re.escape(PART)}"[^>]*?pin="([^"]+)"', body)
            if pr:
                owned.append((pr.group(1), sm.group(0),
                              base + sm.start(), base + sm.end(), nm.group(1)))

    have = {o[0] for o in owned}
    print(f"found {len(owned)} existing {PART} segments")

    for pin in TARGET:
        if pin not in sym:
            sys.exit(f"target pin {pin!r} is not on symbol {SYMBOL}")

    # --- phase 1: extract (remove every PART segment) ---------------------
    keep = {}           # pin -> segment text, for pins that get re-inserted
    dropped = []
    for pin, text, s, e, net in owned:
        if pin in NC:
            dropped.append((pin, net))
        else:
            keep[pin] = text
    for _, _, s, e, _ in sorted(owned, key=lambda o: -o[2]):
        sch = sch[:s] + sch[e:]

    # --- phase 2: build stubs for pins that were never wired --------------
    created = []
    for pin, want in TARGET.items():
        if pin in have:
            continue
        geom = _geom.match_neighbor_geometry(pin, inst, sym, existing)
        keep[pin] = _geom.render_segment(PART, pin, geom)
        created.append((pin, want))

    # --- phase 3: re-insert every segment into its target net ------------
    moved = same = 0
    was = {o[0]: o[4] for o in owned}
    for pin, text in sorted(keep.items(), key=lambda kv: kv[0]):
        want = TARGET[pin]
        if was.get(pin) == want:
            same += 1
        else:
            moved += 1
        blocks = {n: (a, b) for n, a, b in net_blocks(sch)}
        if want in blocks:
            _, end = blocks[want]
            sch = sch[:end] + text + sch[end:]
        else:
            close = sch.rfind("</nets>")
            if close < 0:
                sys.exit("no </nets> found")
            sch = (sch[:close]
                   + f'<net name="{want}" class="0">\n{text}</net>\n'
                   + sch[close:])
            print(f"  created net {want}")

    # --- phase 4: drop nets left with no segments ------------------------
    emptied = []
    for name, a, b in reversed(net_blocks(sch)):
        if "<segment>" not in sch[a:b]:
            emptied.append(name)
            blk = re.search(rf'<net name="{re.escape(name)}" class="0">.*?</net>\s*',
                            sch, re.DOTALL)
            sch = sch[:blk.start()] + sch[blk.end():]

    SCH.write_text(sch, encoding="utf-8")

    print(f"re-inserted {len(keep)} segments: {moved} moved, {same} already correct")
    print(f"created {len(created)} new stubs: " +
          ", ".join(f"{p}->{n}" for p, n in sorted(created)))
    print(f"disconnected {len(dropped)}: " +
          ", ".join(f"{p} (was {n})" for p, n in sorted(dropped)))
    if emptied:
        print(f"removed {len(emptied)} emptied nets: {', '.join(emptied)}")


if __name__ == "__main__":
    main()
