# TerribleFire PCMCIA Driver

Amiga device driver and boot ROM for the TerribleFire PCMCIA board — a custom PCMCIA card for the Amiga 1200 featuring a CH32V307 MCU, SD card slot, and 4MB SPIRAM.

## Overview

The board plugs into the A1200's PCMCIA slot and provides:

- **4MB SPIRAM** mapped into the PCMCIA common memory space ($600000–$9FFFFF), usable as fast RAM
- **SPI SD card interface** exposed through I/O space registers
- **128KB boot ROM** in attribute memory that loads and initializes the device driver

The CH32V307 MCU on the board handles SPI bus arbitration and SD card communication. The Amiga CPU communicates with it through memory-mapped registers in the PCMCIA I/O space.

## Memory Map

### Common Memory ($600000–$9FFFFF)

4MB SPIRAM, always available. Added as expansion memory by the device driver.

### Attribute Memory ($A00000–$A1FFFF)

- `$A00000`–`$A001FE`: CIS tuples (even bytes only, switched via BOARD_CTRL)
- `$A00200`+: 128KB boot ROM (read-only, contains DIAG entry, RomTag, and embedded device driver)

### I/O Space Registers ($A20000+)

All registers are at even byte addresses (68000 bus convention):

| Address    | Name         | R/W | Description |
|------------|--------------|-----|-------------|
| `$A20200`  | `SPI_DATA`   | R/W | SPI data register — write a byte to clock it out, read to get the byte clocked in |
| `$A20202`  | `SPI_CS`     | W   | SPI chip select — `$00` = assert (active), `$FF` = deassert |
| `$A20204`  | `SPI_STATUS` | R   | Status register — bit 0: SD card detect (1 = card present) |
| `$A20206`  | `BOARD_CTRL` | R/W | Board control — bit 0: CIS select (0 = DIAG CIS, 1 = XIP CIS) |
| `$A20208`  | `BOARD_ID`   | R   | Board identification — reads `$01` |

## Boot Sequence

The board uses a dual-CIS scheme with DIAG and XIP phases:

### Phase 1: DIAG (coldstart, BOARD_CTRL=0)

1. **Coldstart** enables the credit card interface and reads CIS at `$A00000`
2. Finds `CISTPL_AMIGAXIP` with flag=`$23` (DIAG mode) and a 4-byte offset
3. JMPs to `$600000 + offset` = `$A00200` (DIAG entry in attribute memory)
4. DIAG entry (`BRA.W DiagHandler`) runs the DIAG handler which:
   - Writes BOARD_CTRL to switch CIS from DIAG to XIP
   - Returns to coldstart via `JMP (A5)`
5. Coldstart disables the credit card interface and continues boot

### Phase 2: XIP (after card.resource init, BOARD_CTRL=1)

1. `card.resource` initializes and re-enables the credit card interface
2. `IfAmigaXIP()` reads the XIP CIS, finds `CISTPL_AMIGAXIP` with `TP_XIPLOC=$400204` and `TP_XIPFLAGS=$01` (AUTORUN)
3. `strap/boot` calls `InitResident()` on the RomTag at `$A00204`
4. **XIP init code**:
   - Parses the embedded `tfpcmcia.device` (Amiga hunk executable) from ROM
   - Allocates RAM, copies hunks, applies HUNK_RELOC32 relocations
   - Flushes the CPU caches (`CacheClearU`)
   - Scans for a RomTag in the loaded code and calls `InitResident()` on it
5. **Device init** — `tfpcmcia.device` registers itself, initializes the SD card via SPI, creates an I/O task, adds SPIRAM as expansion memory, then scans the SD card for an RDB (Rigid Disk Block)
6. **RDB mounting** — Reads partition table, loads any filesystem handlers from LSEG blocks, creates DOS nodes via `MakeDosNode`/`AddBootNode`
7. **DOS boot** — Calls `InitResident("dos.library")` to start AmigaDOS, which picks the highest-priority bootable partition

If no bootable partition is found, InitResident returns and the XIP init exits cleanly. The device driver and SPIRAM remain available.

## Boot ROM Layout (attribute memory $A00200+)

```
$A00200: BRA.W DiagHandler     ; DIAG entry (4 bytes)
$A00204: RomTag                ; XIP entry ($4AFC matchword)
         ...
         Init                  ; XIP init function
         DiagHandler           ; switches CIS, returns via JMP (A5)
         DeviceBinary          ; embedded tfpcmcia.device
         SimonTopDog           ; early boot code
```

## MAME Testing

The board is emulated in MAME as the `tfpcmcia` PCMCIA device. To test with the A1200 emulation:

```sh
mame a1200 -pcmcia tfpcmcia -hard2 sdcard.hdf -debug
```

The MAME emulation (`tfpcmcia.cpp`) implements the full register set, SPI SD card interface, boot ROM, dual CIS switching, and SPIRAM. The boot ROM binary is loaded from the MAME ROM path.

To see the driver's serial debug output (`kprintf`), add a null modem on the RS-232 port and listen with netcat:

```sh
mame a1200 -pcmcia tfpcmcia -hard2 sdcard.hdf -debug \
  -rs232 null_modem -bitb socket.127.0.0.1:1234
```

In another terminal:

```sh
nc -l 1234
```

To build a new ROM and update MAME's copy:

```sh
cd driver
make
cp tfpcmcia.rom ../path/to/mame/roms/tfpcmcia/
```
