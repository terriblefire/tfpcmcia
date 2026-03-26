# tfpcmcia

TerribleFire PCMCIA — a custom PCMCIA expansion card for the Amiga 1200 and Amiga 600, providing 4 MB fast RAM, an SD card interface, and 7 programmable RGB LEDs.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | WCH CH32V307 (RISC-V, 144 MHz) | — |
| SPI RAM | APS6404L-3SQR, 4 MB | SPI1 @ 72 MHz |
| SD card | Standard microSD | SPI3 @ 36 MHz |
| LEDs | 7× APA102C RGB | GPIO (bit-banged) |

The card plugs into the Amiga's PCMCIA slot. The CH32V307 bridges the PCMCIA bus to an SPI RAM chip and SD card. The Amiga CPU accesses them entirely through memory-mapped reads and writes — the MCU handles all bus timing under interrupt.

---

## Memory Map

From the Amiga's perspective:

| Amiga address | Content | Notes |
|---------------|---------|-------|
| `$600000–$61FFFF` | Boot ROM (128 KB) | At power-on only; switches to SPIRAM after boot |
| `$620000–$9FFFFF` | SPIRAM (≈4 MB) | Fast RAM, available after driver init |
| `$A00000–$A001FF` | Attribute memory | PCMCIA CIS tuples (REG=1 accesses) |
| `$A20200–$A2023B` | I/O registers | Control, SPI, LED framebuffer |

The first 128 KB of SPIRAM overlaps the boot ROM window. After driver initialisation, `BOARD_CTRL` bit 0 is set, which switches that window from ROM to SPIRAM and makes the full 4 MB available as expansion memory.

---

## I/O Registers

All registers sit in PCMCIA I/O space. The Amiga's Gayle chip maps card I/O to `$A20000`; register byte offsets are relative to the card's I/O base (`0x220000`). The 68000 bus is 16 bits wide; each register occupies the upper byte of a 16-bit word (big-endian convention).

| Address | Offset | R/W | Name | Description |
|---------|--------|-----|------|-------------|
| `$A20200` | `0x200` | R/W | `SPI_DATA` | Write a byte to clock it out to the SD card; read to get the last byte clocked in |
| `$A20202` | `0x202` | W | `SPI_CS` | SD card chip select — `$00` = assert (active LOW), `$FF` = deassert |
| `$A20204` | `0x204` | R | `SPI_STATUS` | Bit 0: SD card detect (`1` = card present) |
| `$A20206` | `0x206` | R/W | `BOARD_CTRL` | Bit 0: `0` = boot ROM mapped, `1` = SPIRAM mapped; also controls LED mode |
| `$A20208` | `0x208` | R | `BOARD_ID` | Board identifier, always reads `$01` |
| `$A20220–$A2023B` | `0x220–0x23B` | W | `LED_FB` | LED framebuffer — 7 pixels × 4 bytes; see LED section |

---

## Boot Sequence

1. **CIS parsing** — Kickstart's `card.resource` reads the Card Information Structure from attribute memory (`$A00000`) and locates the `CISTPL_AMIGAXIP` (Execute-In-Place) tuple with the `AUTORUN` flag.

2. **XIP entry** — Kickstart calls `InitResident()` on the RomTag at `$600000`.

3. **Boot ROM init** — The ROM code:
   - Parses the embedded `tfpcmcia.device` (standard Amiga HUNK executable) from the ROM image
   - Allocates chip RAM, copies hunks, applies `HUNK_RELOC32` relocations
   - Calls `CacheClearU()` to flush CPU caches
   - Scans the loaded code for a RomTag and calls `InitResident()` on it

4. **Device init** — `tfpcmcia.device`:
   - Registers itself with Exec
   - Initialises the SD card via SPI (supports SDv1, SDv2, SDHC)
   - Creates an I/O task for asynchronous SD operations
   - Probes SPIRAM and, if present, adds a 1 MB block to the system free memory list (`AddMemList`, `MEMF_PUBLIC | MEMF_FAST`)
   - Scans the SD card for an RDB (Rigid Disk Block)

5. **RDB mounting** — Reads the partition table, loads filesystem handlers from LSEG blocks, creates DOS nodes via `MakeDosNode` / `AddBootNode`.

6. **DOS boot** — Calls `InitResident("dos.library")`. AmigaDOS picks the highest-priority bootable partition and continues the boot.

7. **ROM→SPIRAM switch** — After setup, the boot code copies a small trampoline onto the stack, flushes caches, jumps to the trampoline, then writes `BOARD_CTRL` bit 0 to switch the `$600000` window from ROM to SPIRAM. The full 4 MB is now available as fast RAM.

### A600 notes

On the A600 with Kickstart 3.0, the Gayle PCMCIA disable bit must be toggled in the DIAG handler to enable XIP boot. This is handled automatically by the boot ROM.

---

## SPIRAM

The 4 MB SPI RAM (APS6404L) is cached inside the CH32V307's 32 KB SRAM using a direct-mapped write-back cache:

- **512 cache lines × 64 bytes** = 32 KB cache
- **Address decomposition:** tag (7 bits) | line index (9 bits) | byte offset (6 bits)
- On a cache miss, any dirty line is evicted (written back via SPI burst) before the new line is fetched
- SPI transfers use 16-bit burst mode for maximum throughput (~9 MB/s)

From the Amiga driver's perspective, SPIRAM is transparent: ordinary memory reads and writes go through the cache without any special handling.

---

## LED System

Seven APA102C RGB LEDs are driven from two GPIO lines (clock + data) using a bit-banged serial protocol. The active LED mode is determined by `BOARD_CTRL` bit 0.

### KITT mode (bit 0 = 0, default)

Before the driver loads — and any time `BOARD_CTRL` bit 0 is clear — the firmware runs an autonomous scanner animation: a red spot bounces back and forth across all 7 LEDs with a short brightness tail, updating once per millisecond.

### Framebuffer mode (bit 0 = 1)

Once the driver sets `BOARD_CTRL` bit 0, the firmware switches to host-controlled mode. The Amiga driver paints arbitrary colours by writing to the `LED_FB` registers at `$A20220`.

#### LED framebuffer format

7 pixels, 4 bytes each (28 bytes total, `$A20220–$A2023B`). Each pixel occupies two consecutive 16-bit I/O writes:

| Write | Address offset | Upper byte [15:8] | Lower byte [7:0] |
|-------|---------------|-------------------|------------------|
| 1st | `0x220 + pixel×4` | Red | Green |
| 2nd | `0x222 + pixel×4` | Blue | Unused |

Pixel 0 is at `$A20220`, pixel 1 at `$A20224`, …, pixel 6 at `$A20238`.

---

## PCMCIA Interface (firmware)

The CH32V307 services every PCMCIA bus cycle under interrupt (`WCH-Interrupt-fast`). The interrupt handler runs entirely from RAM (`.ramtext` section) and is flattened / always-inlined to avoid register corruption in the fast-interrupt ABI.

Cycle types handled:

| Cycle | Action |
|-------|--------|
| Attribute memory read | Returns CIS tuple bytes (see below) |
| Common memory read, addr < 128 KB | Returns boot ROM byte pair |
| Common memory read, addr ≥ 128 KB | Returns SPIRAM data (cached) |
| Common memory write | Writes to SPIRAM (cached) |
| I/O read | Returns register value |
| I/O write | Updates register or LED framebuffer |

The Gayle chip swaps D0–D7 with D8–D15 relative to the 68000. Every value written to the data bus goes through `BUS16()` to compensate.

### CIS tuples (attribute memory)

| Tuple | Content |
|-------|---------|
| `CISTPL_DEVICE` (0x01) | SRAM, 250 ns, 4 MB |
| `CISTPL_VERS_1` (0x15) | Manufacturer: "TerribleFire", Product: "PCMCIA SD+RAM", Version: "1.0" |
| `CISTPL_FUNCID` (0x21) | Memory card |
| `CISTPL_AMIGAXIP` (0x91) | Execute-in-place, AUTORUN flag set |
| `CISTPL_END` (0xFF) | — |

---

## Amiga Driver (`tfpcmcia.device`)

Standard Amiga block device. Supports:

- `TD_READ64` / `TD_WRITE64` / `TD_SEEK64`
- `HD_SCSICMD`
- `NSCMD_DEVICEQUERY` / `NSCMD_TD_READ64` / `NSCMD_TD_WRITE64`

SD card geometry presented to the OS: 512-byte blocks, 16 heads, 32 sectors/track.

---

## Building

### Firmware

Open the project in MounRiver Studio (or use the provided Makefile under `firmware/`) and build for CH32V307. Flash with WCH-ISP (macOS binary included in `wchisp-macos-x64/`).

### Driver / boot ROM

```sh
cd driver
make
```

Produces `tfpcmcia.rom` — the 128 KB image that the firmware exposes as the boot ROM.

### MAME testing

The board is emulated as the `tfpcmcia` PCMCIA device:

```sh
mame a1200 -pcmcia tfpcmcia -hard2 sdcard.hdf -debug
```

To capture the driver's serial debug output:

```sh
mame a1200 -pcmcia tfpcmcia -hard2 sdcard.hdf \
  -rs232 null_modem -bitb socket.127.0.0.1:1234
# in another terminal:
nc -l 1234
```

Copy a freshly built ROM into MAME's ROM path:

```sh
cp driver/tfpcmcia.rom /path/to/mame/roms/tfpcmcia/
```
