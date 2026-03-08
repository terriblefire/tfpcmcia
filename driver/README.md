# TerribleFire PCMCIA Driver

Amiga device driver and boot ROM for the TerribleFire PCMCIA board — a custom PCMCIA card for the Amiga 1200 featuring a CH32V307 MCU, SD card slot, and 4MB SPIRAM.

## Overview

The board plugs into the A1200's PCMCIA slot and provides:

- **4MB SPIRAM** mapped into the PCMCIA common memory space ($600000–$9FFFFF), usable as fast RAM
- **SPI SD card interface** exposed through attribute memory registers
- **64KB boot ROM** that loads and initializes the device driver at cold boot, then switches to SPIRAM mode

The CH32V307 MCU on the board handles SPI bus arbitration and SD card communication. The Amiga CPU communicates with it through memory-mapped registers in the PCMCIA attribute memory space.

## Memory Map

### Common Memory ($600000–$9FFFFF)

At power-on, the first 64KB ($600000–$60FFFF) contains the boot ROM. After the driver initializes, `BOARD_CTRL` bit 0 switches this region to SPIRAM, giving the full 4MB to the system.

### Attribute Memory Registers ($A00000+)

All registers are at even byte addresses (68000 bus convention):

| Address    | Name         | R/W | Description |
|------------|--------------|-----|-------------|
| `$A00200`  | `SPI_DATA`   | R/W | SPI data register — write a byte to clock it out, read to get the byte clocked in |
| `$A00202`  | `SPI_CS`     | W   | SPI chip select — `$00` = assert (active), `$FF` = deassert |
| `$A00204`  | `SPI_STATUS` | R   | Status register — bit 0: SD card detect (1 = card present) |
| `$A00206`  | `BOARD_CTRL` | W   | Board control — bit 0: SPIRAM mode (1 = SPIRAM, 0 = boot ROM) |
| `$A00208`  | `BOARD_ID`   | R   | Board identification — reads `$01` |

## Boot Sequence

1. **CIS parsing** — Kickstart's `card.resource` reads the Card Information Structure from attribute memory and finds the `CISTPL_AMIGAXIP` (Execute-In-Place) tuple
2. **XIP entry** — Kickstart calls `InitResident()` on the RomTag at `$600000`
3. **Boot ROM init** — The ROM code:
   - Parses the embedded `tfpcmcia.device` (Amiga hunk executable) from ROM
   - Allocates RAM, copies hunks, applies HUNK_RELOC32 relocations
   - Flushes the CPU caches (`CacheClearU`)
   - Scans for a RomTag in the loaded code and calls `InitResident()` on it
4. **Device init** — `tfpcmcia.device` registers itself, initializes the SD card via SPI, creates an I/O task, then scans the SD card for an RDB (Rigid Disk Block)
5. **RDB mounting** — Reads partition table, loads any filesystem handlers from LSEG blocks, creates DOS nodes via `MakeDosNode`/`AddBootNode`
6. **DOS boot** — Calls `InitResident("dos.library")` to start AmigaDOS, which picks the highest-priority bootable partition

After initialization, the boot ROM copies a small trampoline to the stack, flushes caches, jumps to it, and writes `BOARD_CTRL` to switch from ROM to SPIRAM mode. The 4MB SPIRAM is then available as expansion memory.

## MAME Testing

The board is emulated in MAME as the `tfpcmcia` PCMCIA device. To test with the A1200 emulation:

```sh
mame a1200 -pcmcia tfpcmcia -hard2 sdcard.hdf -debug
```

The MAME emulation (`tfpcmcia.cpp`) implements the full register set, SPI SD card interface, boot ROM, and SPIRAM switching. The boot ROM binary is loaded from the MAME ROM path.

To build a new ROM and update MAME's copy:

```sh
cd driver
make
cp tfpcmcia.rom ../path/to/mame/roms/tfpcmcia/
```
