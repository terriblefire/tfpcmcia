# CH32V307VCT6 + APS6404L → CH32V467VET6 Migration Plan

Date: 2026-08-15 (revised 2026-08-22)
Source: `docs/CH32V407DS0.PDF` (CH32V407/V467 Datasheet V1.2)
Status: schematic and board connectivity complete — see §10.

Goal: replace the CH32V307VCT6 **and** the external APS6404L SPI RAM with a single
CH32V467VET6 (8 MB on-chip PSRAM), keeping all existing tfpcmcia functionality,
with **every Amiga-facing pin on a 5 V-tolerant (FT) pad**.

---

## 1. What the silicon actually forces

### 1.1 Part selection

Only one variant is viable:

| Model | Pins | GPIO | PSRAM | Verdict |
|-------|------|------|-------|---------|
| CH32V467**VET6** | LQFP100 14×14 0.5 mm | 76 | **8 MB** | ✅ the only option |
| CH32V467WEU6 | QFN68 8×8 | 54 | 8 MB | ❌ need ~63 GPIO |
| CH32V467RET6 | LQFP64 7×7 | 48 | 4 MB only | ❌ |

The LQFP100 land pattern is **identical to the current CH32V307VCT6** — the footprint
does not change, only the netlist and routing.

Other deltas vs. the V307: 200 MHz core (vs 144), 200 KB SRAM (vs 32/64 KB),
992 KB flash, and PSRAM linearly memory-mapped at **`0x8000_0000`–`0x8080_0000`**.

### 1.2 Package pinout deltas (V307VCT6 → V467VET6)

The differences are confined to pins 38–55 plus 73/74:

| Pin | CH32V307VCT6 | CH32V467VET6 |
|-----|--------------|--------------|
| 38 | PE7 | PE8 |
| 39 | PE8 | PE9 |
| 40 | PE9 | **VDDK** |
| 41 | PE10 | VSS |
| 42–45 | PE11–PE14 | **MDIRN / MDIRP / MDITN / MDITP** (Ethernet PHY) |
| 46 | PE15 | VDD |
| 47 | PB10 | VSS |
| 48 | PB11 | **VDD18** (PSRAM 1.8 V rail — `PE7` on the V407) |
| 49 | VSS | PB10 |
| 50 | VDD | PB11 |
| 51 | PB12 | **PD8** |
| 52–55 | PB13–PB15, PD8 | PB12–PB15 |
| 73 | NC | **PE10** |
| 74 | VSS | **PE11** |

⚠️ **Pins 73/74 are easy to miss.** The current board ties pad 74 to GND (it is
`VSS_2` in the STM32F103VET6 library part). On the V467 that pad is **PE11** — a
GPIO. Leaving it strapped to ground would short an output. Pad 73 goes from NC to
PE10.

**Consequence: `PE7`, `PE13`, `PE14`, `PE15` do not exist on any CH32V407/V467 package.**
Port E is only `PE0–PE6` + `PE8–PE12` = 12 pins.

> The current design puts A0–A15 on PE0–PE15 for an atomic single-port address read.
> **That is no longer possible** — and it can't be recovered by moving things around,
> because Port D (the only remaining contiguous 16-bit port) holds the data bus.
> Address reassembly in the ISR is unavoidable. See §5.

We are not using Ethernet, so pins 42–45 are left NC (no magnetics, no RJ45).

### 1.3 5 V tolerance map (CH32V467VET6)

Datasheet Table 2-1, `I/O structure` column: `FT` = 5 V tolerant, `HS` = 100 MHz
capable. Absolute max input on non-FT pins is `VDD + 0.3 V`.

**5 V tolerant (53 pins):**

| Port | FT pins | Count |
|------|---------|-------|
| A | PA8, PA9, PA10, PA13, PA14, PA15 | 6 |
| B | PB2, PB3, PB4, PB5, PB8–PB15 | 12 |
| C | PC6–PC12 | 7 |
| D | PD0–PD15 | **16** |
| E | PE0–PE6, PE8–PE12 | 12 |

**NOT 5 V tolerant (23 pins):** PA0–PA7, PA11, PA12 (USB1), PB0, PB1,
**PB6, PB7 (USBHS2 D−/D+)**, PC0–PC5, PC13–PC15.

This kills four placements in the current design outright:

- `CE1` on **PB6** and `OE` on **PB7** — these are the USB HS2 PHY pads on this part.
- `A16`/`A17` on **PB0/PB1**.
- `READY` on **PA2**.

---

## 2. Pin budget

Amiga-facing signals: **49**
- A0–A21 (22 in), D0–D15 (16 bidir), CE1/CE2/OE/WE/IORD/IOWR/REG/RESET (8 in),
  WAIT/READY/IOCS16 (3 out).

All 49 land on FT pads. The BVD2 audio output was the one exception; the audio stage
has since been removed from the design, so there is no longer any non-FT Amiga-facing
signal at all (see §6.4).

Available FT pins: **53**, less PD0–PD15 for the data bus = 37, less PE12 (BOOT0,
needs a pull-down and must stay low at power-up) = 36, less PA13/PA14 (SWD) = **34**.

**34 available vs 33 needed. One spare.** It fits — but only because dropping the
SPI RAM frees SPI1 (PA5/PA6/PA7, non-FT) for the SD card, which in turn releases
PC10/PC11/PC12 (FT) for the bus. That swap is what makes the whole thing close.

Everything that isn't Amiga-facing fits comfortably on the 23 non-FT pins:
SD SPI1, SD_CS, SD_CD, LED clk/data, 2 status LEDs, debug UART — 11 of 23 used.

---

## 3. Option A — pure re-map, no external glue *(recommended)*

All 33 5 V signals on FT pads. No added parts. Removes the APS6404L from the BOM.
See §7 for the full map and §7.1 for how the layout was derived.

**Pros:** no BOM additions, SWD retained, A0 retained, identical footprint, every
control-signal bitmask in `pcmcia.h` survives unchanged.
**Cons:** 1 spare FT pin (PB2); address reassembly costs ~12 instructions in the
ISR instead of 5, and the ISR needs a third GPIO read.

## 4. Option B — Option A plus slack

Two independent levers, either or both:

- **Drop A0.** It is already masked off in every firmware path
  (`RomMask = 0x1FFFE`, `SpiRamMask = 0x3FFFFE`); byte selection comes from
  UDS/LDS. Frees 1 FT pin.
- **Give up SWD** (PA13/PA14) and program via the WCH USB bootloader plus the debug
  UART. Frees 2 FT pins. ⚠️ **The USB breakout (JP1) has since been removed**, so this
  lever now costs a board change to expose PA11/PA12 again — it is no longer free.

Taking both gives **3–4 spare FT pins**, which is the difference between "fits" and
"fits with room for a future signal". Recommended if you want any expansion capacity.

## 5. Option C — external address buffers *(considered, not recommended)*

Buffering A0–A21 through 2× 74LVC16244 (5 V-input tolerant at VCC = 3.3 V) would
let address bits land on non-FT pins and free up FT pads.

**But it buys nothing structurally.** The atomic 16-bit address read cannot be
restored at any price — Port D is the data bus and Port E is truncated to 12 pins,
so no port can hold A0–A15 regardless of what the buffers allow. You would pay two
extra ICs, board area on a PCMCIA card, and ~4 ns of propagation delay for pin
freedom you don't actually need once the SD card moves to SPI1.

Skip it.

---

## 6. Orthogonal decisions (independent of pin mapping)

### 6.1 8 MB PSRAM vs a 4 MB window

The Amiga PCMCIA common-memory window (`$600000`–`$9FFFFF`) is 4 MB, and the card
only receives A0–A21. **You cannot linearly expose more than 4 MB**, so the extra
4 MB needs a decision:

- **6.1a** Map 4 MB linearly (drop-in behaviour), use the other 4 MB as an SD read
  cache / RAM disk. *Recommended* — simple, and AmigaOS gets a normal contiguous
  `AddMemList` block.
- **6.1b** Bank-switch the upper 4 MB via a `BOARD_CTRL` bit. Gives 8 MB of storage
  but banked RAM is useless as system fast RAM.

**The real win here is not capacity, it's latency** — see 6.2.

### 6.2 Firmware: delete the SPI RAM cache entirely

PSRAM is linearly memory-mapped at `0x8000_0000`, so `SPIRAM_Read16()` collapses to
a single load:

```c
*(volatile uint16_t *)(0x80000000u + (addr & 0x3FFFFEu))
```

That deletes `spiram.c` / `spiram.h`, including the 32 KB direct-mapped write-back
cache. Today a cache miss costs a 64-byte SPI burst at 72 MHz ≈ **7 µs**, doubled to
~14 µs if the evicted line is dirty — the Amiga sits in `/WAIT` for all of it.
PSRAM `tRC` is **60 ns**.

This dwarfs everything else. The ~30 ns of extra address reassembly (~12 instructions
at 200 MHz vs 5 at 144 MHz) is noise by comparison, and `/WAIT` is asserted at the top
of the handler anyway, so the cycle extends rather than failing.

### 6.3 ⚠️ PSRAM lot-code caveat — verify before ordering

Datasheet note under Table 3-42:

> Only for CH32V467 chips **with the fifth last digit of the lot number not 0**, PSRAM
> can support clocks exceeding 200 MHz, support variable and instruction access, and
> **support byte writing**.

**Byte writing matters directly**: `PCMCIA_Handler` does `SPIRAM_Write8()` on UDS-only
and LDS-only cycles. On an older lot you would need a read-modify-write (2 PSRAM
cycles, ~120 ns) instead of a single store. Confirm the lot code with the supplier, or
budget for the RMW path.

### 6.4 Audio output — removed

*Superseded.* The original design drove PCMCIA pin 62 (BVD2) from DAC1_OUT on PA4
through an MCP6001 buffer. DAC1_OUT is PA4 and DAC2_OUT is PA5; both are non-FT with
no remap, so this was the one Amiga-facing signal that could not sit on a 5 V-tolerant
pad, and it needed AC-coupling or a series clamp resistor.

**The audio stage was deleted** (IC4 + C13/C14 + R4/R5/R6). With it gone, PA4 is free
and **every Amiga-facing pin is now on an FT pad** — no coupling caps, no clamp
resistors, no compromise. PCMCIA pin 62 is left entirely unconnected.

If audio is ever wanted back, it returns as this same problem: DAC1 on PA4 (PA5 is
SPI1_SCK and belongs to the SD card), AC-coupled into the mixer.

### 6.5 Power

- **New:** VDD18 (pin 48) — 0.1 µF ∥ 2.2 µF. This is the PSRAM rail, generated
  internally from VDD by LDO_1.8V.
- **Changed:** main VDD bulk must rise from ≥4.7 µF to **≥10 µF**, and total VDD
  capacitance must be ≥ (VDDK + VDD18 capacitance).
- VDDK (pin 40) unchanged: 0.1 µF ∥ 2.2 µF.
- 8 MB of PSRAM plus a 200 MHz core is a meaningful current increase over the
  V307 + APS6404L. **Measure Icc against the PCMCIA slot budget** on both A600 and
  A1200 before committing.

### 6.6 BOOT0

BOOT0 is PE12 on this part but still lands on **pin 94**, same as the V307 — the
existing pull-down carries over unchanged. Do not use PE12 as a bus pin: an Amiga
address line driving it high at power-up would drop the card into the bootloader.

---

## 7. Recommended full pin map (Option A)

| Pin | Signal | FT | Note |
|-----|--------|----|------|
| PD0–PD15 | D0–D15 | ✅ | unchanged from current design |
| PE0–PE6 | A0–A6 | ✅ | |
| PE8–PE11 | A8–A11 | ✅ | PE7 does not exist → A7 hole, filled from PB3 |
| PE12 | *BOOT0* | ✅ | pull-down, reserved |
| PB3 | A7 | ✅ | |
| PB4, PB5 | A20, A21 | ✅ | |
| PB8–PB15 | A12–A19 | ✅ | |
| PB2 | *spare* | ✅ | only free FT pin |
| PC6 | CE1 | ✅ | EXTI6 |
| PC7 | OE | ✅ | |
| PC8 | WE | ✅ | |
| PC9 | CE2 | ✅ | EXTI9 |
| PC10 | REG | ✅ | |
| PC11 | IORD | ✅ | |
| PC12 | IOWR | ✅ | |
| PA15 | RESET | ✅ | EXTI15 → `EXTI15_10`, same vector as today |
| PA8 | WAIT | ✅ | output |
| PA9 | READY | ✅ | output |
| PA10 | IOCS16 | ✅ | output |
| PA5/PA6/PA7 | SD_CLK / SD_MISO / SD_MOSI (SPI1) | ❌ | 3.3 V SD card |
| PC4 | SD_CS | ❌ | |
| PC5 | SD_CD | ❌ | moved from PA15 |
| PC0/PC1 | LED_CLK / LED_DO (APA102) | ❌ | as today |
| PB0/PB1 | LED1 / LED2 status | ❌ | |
| PB6/PB7 | debug UART (USART1 remap) | ❌ | *never* connect to the Amiga bus |
| PA13/PA14 | SWDIO / SWCLK | ✅ | retained — with USB gone, the only way in |
| PA4, PA11, PA12 | *spare* | ❌ | freed by removing the audio stage and USB |
| 42–45 | MDIRN/MDIRP/MDITN/MDITP | — | NC, Ethernet unused |

**Spare:** PB2 (FT — the only one), PA0–PA4, PA11, PA12, PC2, PC3, PC13–PC15 (non-FT).

**Programming access:** SWD on PA13/PA14 and the debug UART on PB6/PB7, both brought
out on the 6-way DEBUG header. The USB breakout jumper was removed, so the WCH USB
bootloader is no longer reachable — SWD and UART are the only routes in.

### 7.1 Why this layout — three GPIO reads and three extracts is the floor

Three GPIO reads are unavoidable. 22 address pins + 7 hot control signals = 29
inputs, but the two largest FT ports hold only 12 (PB) + 11 (PE) = 23 pins. So
control needs its own port — and PC6–PC12 is exactly 7 FT pins, a perfect fit.
That leaves **PE (11) + PB (12) = 23 pins for 22 address bits**: there is no choice
about which ports carry the address, and no arrangement gets back to two reads.

Both address ports have gaps — PE breaks at PE7 (1-bit hole), PB breaks at PB6/PB7
(2-bit hole). One shift+mask per port would leave holes in the assembled address.
PE is fully consumed, so PB's 2-bit hole cannot be filled from anywhere else:
**PB needs two extracts, PE needs one. Three is the minimum.**

The optimisation is choosing PB's first shift so that a single extract both fills
PE's hole *and* covers the 8-bit run: at `<< 4`, PB3 lands on A7 and PB8 lands on
A12.

### 7.2 ISR address reassembly

```c
/*  PE0–PE6  -> A0–A6     (in place)
 *  PE8–PE11 -> A8–A11    (in place; PE7 absent -> A7 hole)
 *  PB3      -> A7        (fills the hole, same shift as the run below)
 *  PB8–PB15 -> A12–A19
 *  PB4,PB5  -> A20,A21                                                */
#define ADDR_E_MASK   0x00000F7Fu  /* PE bits 0-6, 8-11                */
#define ADDR_B1_MASK  0x000FF080u  /* after <<4  : A7, A12-A19         */
#define ADDR_B2_MASK  0x00300000u  /* after <<16 : A20, A21 (bare lui) */

uint32_t e = GPIOE->INDR;
uint32_t b = GPIOB->INDR;
uint32_t addr = (e & ADDR_E_MASK)
              | ((b <<  4) & ADDR_B1_MASK)
              | ((b << 16) & ADDR_B2_MASK);
```

~12 instructions vs 5 today: ≈60 ns at 200 MHz against ≈35 ns at 144 MHz. The third
APB read is likely a larger share of that +25 ns than the shifts are.

### 7.3 Control masks are unchanged

Ordering the control signals on PC as above keeps every bitmask in `pcmcia.h`
byte-for-byte identical to the current design:

| Pin | Signal | Mask | Current pin |
|-----|--------|------|-------------|
| PC6 | CE1 | `0x0040` | PB6 |
| PC7 | OE | `0x0080` | PB7 |
| PC8 | WE | `0x0100` | PB8 |
| PC9 | CE2 | `0x0200` | PB9 |
| PC10 | REG | `0x0400` | PB10 |
| PC11 | IORD | `0x0800` | PB11 |
| PC12 | IOWR | `0x1000` | PB12 |

`STROBE_MASK`, `MEM_MASK`, `IO_MASK`, `ACCESS_MASK`, `OE_MASK`, `WE_MASK`,
`REG_MASK`, `IOR_MASK`, `IOW_MASK` all keep their values. CE1/CE2 still land on
EXTI6/EXTI9 → the same `EXTI9_5` vector and the same `SetVTFIRQ()` registration.

The ISR diff reduces to: `GPIOB->INDR` → `GPIOC->INDR` for control, the new address
block, and `WAIT` moving from `GPIOB` to `GPIOA` bit 8.

`RESET_MASK` disappears — RESET is only serviced by its own EXTI handler and was
never read in the hot path.

---

## 8. Firmware work items

1. Rewrite `gpio.h` pin definitions and `gpio.c` port configuration.
2. `pcmcia.c`: new address reassembly (§7.2); control reads move from `GPIOB->INDR`
   to `GPIOC->INDR` — **mask values are unchanged** (§7.3); `WAIT` assert/deassert
   moves from `GPIOB` bit 14 to `GPIOA` bit 8.
3. EXTI: retarget lines 6 and 9 from Port B to Port C in AFIO (vector stays
   `EXTI9_5`); RESET from PB13 → PA15, i.e. EXTI13 → EXTI15, vector stays
   `EXTI15_10` — update the `EXTI->INTFR = EXTI_Line13` clear in
   `PCMCIA_ResetHandler`.
4. Delete `spiram.c` / `spiram.h`; replace with direct `0x8000_0000` accesses.
5. SD card: SPI3 (PC10–PC12) → SPI1 (PA5–PA7), CS PC9 → PC4, CD PA15 → PC5.
   See §8.1.
6. Clock tree: 144 MHz → 200 MHz; re-check flash wait states and the `DELAY_100NS` /
   `DELAY_140NS` nop counts, which are hand-tuned for 144 MHz and **will be wrong**.
7. PSRAM init: enable LDO_1.8V, configure the PSRAM controller, size/probe 8 MB.
8. Switch the SDK from `ch32v30x` to the V4xx peripheral library.
9. **LED count 7 → 8.** An eighth APA102 (LED10) was added to the chain. Update
   `led_fb[7]` and the `REG_LED_FB_BASE + 28u` window in `pcmcia.c`, and
   `KITT_NUM_LEDS` in `led.c`. The I/O framebuffer window grows to
   `$A20220–$A2023F`; the README register table needs the same change.

### 8.0 Port status (2026-08-23, rev 2 — official V4x7 SDK)

Ported to the **official WCH CH32V4x7 SDK** (`openwch/ch32v407_ch32v467`,
`EVT/EXAM/SRC`), replacing the earlier V307-SDK build: the V4x7 clock tree
(SYSPLL sources, HPRE divider, 20 MHz HSI) is materially different from the
V307 and the reference manual (`docs/CH32V407RM.PDF`, fetched from WCH) plus
the SDK's PSRAM example made the real init requirements clear. Builds clean in
the `terriblefire78/mrs` Docker image. Key facts and choices:

- **PSRAM needs explicit init** — it is NOT usable out of reset. `PSRAM_Init()`
  enables the HB clock, programs timing (TRC=0x0C = 60 ns at 200 MHz) and sets
  the 200M read/write latencies. Read latency is **fixed** (variable-latency
  read is a good-lot-only feature per DS Table 3-42); switch to
  `Read_Variable` on confirmed lots.
- **PSRAM is clocked by SYSCLK** (pre-HPRE). Custom clock config
  `SYSCLK_200MHz_HCLK_200MHz_HSI`: HSI 20 MHz × PLL 10 = 200 MHz, HPRE÷1 —
  **core AND PSRAM both at their any-lot 200 MHz maxima**. The ISR runs 39%
  faster than the proven 144 MHz V307 timing; nop delays rescaled (14/20).
- **Vector table verified**: EXTI9_5_IRQn=39, EXTI15_10_IRQn=56 — identical to
  the V307, so the EXTI binding carries over unchanged.
- **SD on SPI1 at 12.5 MHz** (PCLK2 200/16; SD spec allows 25).
- `PSRAM_Write8` is a 16-bit read-modify-write — safe on every lot (§6.3).
- RVV CSR helpers in `core_riscv.c` are compiled out (GCC 8.2 assembler
  lacks V-extension CSRs; the firmware does not use RVV).
- Linker: RAM origin 0x20000000+1 K (SDK reservation), 135 K — valid under
  both flash/RAM option-byte configs (RM Table 32-4).
- `main.c` prints a **PSRAM probe** result at boot, banner first.

Build: `docker run --rm --platform linux/amd64 -v $PWD:/host terriblefire78/mrs make -C /host/firmware`

### 8.1 SD card interface

Default-mapped **SPI1 (PA5/PA6/PA7)** is both the fastest option and the only one
available:

- SPI2 is on PB12–PB15, SPI3 on PA15/PB3–PB5 (remap PC10–PC12), and SPI1's own
  remap is PB3/PB4/PB5 — every one of those pins is taken by the Amiga bus.
- SPI1 sits on APB2. On this part **both APB buses max out at FHCLK** (Table 3-2:
  `FPCLK1`/`FPCLK2` max = `FHCLK`), unlike the V307 where APB1 was capped at
  HCLK/2. With PCLK2 = 200 MHz, SPI1 ÷4 gives **50 MHz** — exactly the SD
  high-speed ceiling, up from 36 MHz today.

**SDIO is not possible.** `SDIO_CMD` is fixed to **PD2** in all three mappings
(SDIO_RM=0, SDIOEN&ETHMACEN, SDIO_RM=1) — that is data bus D2 — and `SDIO_CK` is
fixed to PC12. There is no escape remap.

**The SPI clock is not the bottleneck.** `driver/spi.c:210` transfers a sector as
512 individual `SpiByte()` calls, each one a PCMCIA I/O cycle plus ISR entry on the
Amiga side. 36 → 50 MHz saves ~60 ns on a byte that costs the better part of a
microsecond. The real opportunity the PSRAM opens up is a **block-transfer path**:
the MCU reads a whole sector into PSRAM itself and the Amiga reads it back as
memory, instead of 512 register round-trips. Worth doing as a follow-up once the
port is functional — it does not need to block the hardware decision.

## 9. To verify before committing to a layout

- [x] Schematic rewired to the §7 map and board contactrefs synced — verified by
      `eagle/check_pinmap.py` (81/81) and a full net↔signal annotation cross-check.
- [x] VDDK, VDD18 and VDD decoupling placed (§6.5).
- [ ] Spot-check pins 38–55 and the FT column against the PDF pages directly — this
      plan's pin numbers were extracted from the PDF text layer.
- [ ] PSRAM lot code (§6.3): prefer fifth-last digit ≠ 0 (byte writes,
      variable-latency reads, >200 MHz). Optional — the firmware is lot-safe as
      built (RMW byte writes, fixed read latency).
- [x] Current draw vs the PCMCIA slot budget (1.0 A @ 5 V): CH32V467 worst case
      61 mA (DS Table 3-6 + V467 note: 39.1 mA all-peripherals @200 MHz + 22 mA
      PSRAM), ≈ current-neutral vs the V307+APS6404L it replaces. Whole-card
      realistic worst ≈ 300 mA (SD write burst + LEDs); host-commanded all-white
      LED ceiling ≈ 700 mA — all inside budget, and all dominated by loads that
      existed on the proven previous revision.
- [ ] Confirm the CH32V4xx peripheral library / toolchain support in the existing
      Docker build.
- [ ] Re-route the board (~40 signals were invalidated by the pin permutation).

## 10. Status

| Area | State |
|------|-------|
| Eagle library part `CH32V467VET6` | done — reuses `QFP50P1600X1600X160-100N` |
| Schematic pin map | done — 81/81 against §7 |
| Board contactrefs | done — 95 nets ↔ 95 signals, no mismatches |
| SPI RAM (IC3) removed | done |
| Audio stage (IC4) removed | done — see §6.4 |
| USB breakout (JP1) removed | done — SWD + UART are the only programming path |
| VDDK / VDD18 / VDD decoupling | done |
| Board routing | done — verified all 81 IC1 pads have copper |
| Firmware port (§8) | done — official V4x7 SDK, 200 MHz core+PSRAM, PSRAM init implemented (§8.0) |
| 8th APA102 (LED10) added | done — `PCMCIA_NUM_LEDS 8`, I/O window now `$A20220–$A2023F` (README table still to update) |

`eagle/check_pinmap.py` re-runs the pin-map check against the live schematic at any
time; it exits non-zero on any deviation.
