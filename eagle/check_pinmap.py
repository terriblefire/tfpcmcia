#!/usr/bin/env python3
"""Check IC1 wiring in tfpcmcia.sch against the CH32V467VET6 target pin map.

Target map is docs/ch32v467-migration-plan.md section 7.
Reads the schematic directly, so no netlist regeneration is needed.

    .venv/bin/python check_pinmap.py [tfpcmcia.sch]

Exit status is 0 only when every pin matches.
"""
import sys
from pathlib import Path
from lxml import etree

HERE = Path(__file__).resolve().parent
SCH = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "tfpcmcia.sch"
LBR = HERE / "SamacSys_Parts.lbr"
PART = "IC1"

# --- target: pin name -> net name ----------------------------------------
TARGET = {}
for i in range(16):
    TARGET[f"PD{i}"] = f"D{i}"
for i in range(7):
    TARGET[f"PE{i}"] = f"A{i}"                       # PE0-PE6  -> A0-A6
TARGET["PB3"] = "A7"                                 # fills the PE7 gap
for i in range(4):
    TARGET[f"PE{8+i}"] = f"A{8+i}"                   # PE8-PE11 -> A8-A11
for i in range(8):
    TARGET[f"PB{8+i}"] = f"A{12+i}"                  # PB8-PB15 -> A12-A19
TARGET["PB4"], TARGET["PB5"] = "A20", "A21"
TARGET.update({                                      # control in
    "PC6": "CE1", "PC7": "OE", "PC8": "!WE", "PC9": "CE2",
    "PC10": "!REG", "PC11": "IORD", "PC12": "IOWR", "PA15": "RESET",
})
TARGET.update({                                      # control out
    "PA8": "!WAIT", "PA9": "READY", "PA10": "WP/!IOCS16",
})
TARGET.update({                                      # peripherals
    "PA5": "SD_CLK", "PA6": "SD_MISO", "PA7": "SD_MOSI",
    "PC4": "SD_SNSS", "PC5": "SD_CD",
    "PC0": "LED_CLK", "PC1": "LED_DO",
    "PB0": "LED1", "PB1": "LED2",
    "PB6": "USART_TX", "PB7": "USART_RX",
    "PA13": "SWDIO", "PA14": "SWCLK",
    "PE12-BOOT0": "BOOT0", "NRST": "NRST",
})
for p in ("VSS_1", "VSS_2", "VSS_3", "VSS_4", "VSS_5", "VSSA", "VREF-"):
    TARGET[p] = "GND"
# BOOT1 must be strapped low: with BOOT0 high (JP2 fitted for ISP) a floating
# BOOT1 leaves the boot source indeterminate. Datasheet Note 5 also asks for a
# pull-down to avoid extra current in low-power modes.
TARGET["PB2-BOOT1"] = "GND"
for p in ("VDD_1", "VDD_2", "VDD_3", "VDD_4", "VDD_MAIN", "VDDA", "VREF+", "VBAT"):
    TARGET[p] = "VCC33"
TARGET["VDDK"] = "VDDK"      # own rail, 0.1uF || 2.2uF to GND
TARGET["VDD18"] = "VDD18"    # own rail, 0.1uF || 2.2uF to GND

# pins that must be left unconnected
NC = {
    "MDIRN": "Ethernet unused", "MDIRP": "Ethernet unused",
    "MDITN": "Ethernet unused", "MDITP": "Ethernet unused",

    "PA0-WKUP": "spare", "PA1": "spare", "PA2": "spare", "PA3": "spare",
    # audio output stage (IC4/C13/C14) removed - BVD2 no longer driven
    "PA4": "spare (audio stage removed)",
    # USB breakout jumper JP1 removed - programming is SWD + UART only
    "PA11": "spare (USB removed)", "PA12": "spare (USB removed)",
    "PC2": "spare", "PC3": "spare",
    "PC13-TAMPER-RTC": "unused", "PC14-OSC32_IN": "unused",
    "PC15-OSC32_OUT": "unused",
    "OSC_IN": "unused (internal RC)", "OSC_OUT": "unused (internal RC)",
}

# non-5V-tolerant pins: nothing on the Amiga bus may land here
NON_FT = ({f"PA{i}" for i in range(8)} | {"PA0-WKUP", "PA11", "PA12"}
          | {"PB0", "PB1", "PB6", "PB7"} | {f"PC{i}" for i in range(6)}
          | {"PC13-TAMPER-RTC", "PC14-OSC32_IN", "PC15-OSC32_OUT"})
AMIGA = ({f"A{i}" for i in range(22)} | {f"D{i}" for i in range(16)}
         | {"CE1", "CE2", "OE", "!WE", "!REG", "IORD", "IOWR", "RESET",
            "!WAIT", "READY", "WP/!IOCS16"})


def main():
    root = etree.parse(str(SCH)).getroot()
    lbr = etree.parse(str(LBR)).getroot()
    pad = {c.get("pin"): int(c.get("pad")) for c in
           lbr.find(".//deviceset[@name='CH32V467VET6']").findall(".//connect")}

    actual = {}
    for net in root.findall(".//net"):
        for seg in net.findall("segment"):
            for pr in seg.findall("pinref"):
                if pr.get("part") == PART:
                    actual.setdefault(pr.get("pin"), []).append(net.get("name"))

    wrong, missing, extra, dupe = [], [], [], []
    for pin, want in sorted(TARGET.items(), key=lambda kv: pad.get(kv[0], 0)):
        got = actual.get(pin)
        if got is None:
            missing.append((pad.get(pin), pin, want))
        elif len(got) > 1:
            dupe.append((pad.get(pin), pin, got))
        elif got[0] != want:
            wrong.append((pad.get(pin), pin, got[0], want))
    for pin, why in sorted(NC.items(), key=lambda kv: pad.get(kv[0], 0)):
        if pin in actual:
            extra.append((pad.get(pin), pin, actual[pin][0], why))

    unsafe = [(pad.get(p), p, n[0]) for p, n in actual.items()
              if p in NON_FT and n[0] in AMIGA]

    ok = len(TARGET) - len(wrong) - len(missing) - len(dupe)
    print(f"IC1 / CH32V467VET6 pin map: {ok}/{len(TARGET)} correct\n")

    def show(title, rows, fmt):
        if rows:
            print(f"{title} ({len(rows)})")
            for r in rows:
                print("   " + fmt(r))
            print()

    show("UNSAFE - Amiga bus signal on a non-5V-tolerant pin", unsafe,
         lambda r: f"pad {r[0]:>3}  {r[1]:<16} {r[2]}")
    show("WRONG NET", wrong,
         lambda r: f"pad {r[0]:>3}  {r[1]:<16} {r[2]:<14} -> should be {r[3]}")
    show("NOT CONNECTED", missing,
         lambda r: f"pad {r[0]:>3}  {r[1]:<16} needs {r[2]}")
    show("SHOULD BE UNCONNECTED", extra,
         lambda r: f"pad {r[0]:>3}  {r[1]:<16} on {r[2]:<14} ({r[3]})")
    show("PIN ON MULTIPLE NETS", dupe,
         lambda r: f"pad {r[0]:>3}  {r[1]:<16} {r[2]}")

    bad = bool(wrong or missing or extra or dupe or unsafe)
    print("FAIL" if bad else "PASS - pin map matches the target")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
