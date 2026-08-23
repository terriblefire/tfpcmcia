# CH32V467VET6 rewiring checklist

Generated from `eagle/tfpcmcia.net` against the target map in
`ch32v467-migration-plan.md` §7.

> **The part swap kept every net on its original pad.** Because the CH32V467VET6
> has a different pinout in the 38–55 and 73/74 ranges, the existing wiring is now
> attached to the wrong functions — including supply pins driven by bus signals.
> Nothing here should be routed or fabricated until this list is worked through.


## Summary

- 43 pins already correct
- **41 pins need rewiring**
- 7 pins currently wired that must be disconnected


## Pins to disconnect

| Pad | Pin | Currently | Action |
|-----|-----|-----------|--------|
| 24 | `PA1` | `RAM_SNSS` | NC (spare) |
| 25 | `PA2` | `READY` | NC (spare) |
| 37 | `PB2-BOOT1` | `A18` | NC (spare (only free 5V-tolerant pin)) |
| 42 | `MDIRN` | `A11` | NC (Ethernet unused) |
| 43 | `MDIRP` | `A12` | NC (Ethernet unused) |
| 44 | `MDITN` | `A13` | NC (Ethernet unused) |
| 45 | `MDITP` | `A14` | NC (Ethernet unused) |


## Pins to rewire

| Pad | Pin | Currently | Target net |
|-----|-----|-----------|------------|
| 27 | `VSS_2` | *(unconnected)* | **`GND`** |
| 28 | `VDD_2` | *(unconnected)* | **`VCC33`** |
| 30 | `PA5` | `RAM_CLK` | **`SD_CLK`** |
| 31 | `PA6` | `RAM_MISO` | **`SD_MISO`** |
| 32 | `PA7` | `RAM_MOSI` | **`SD_MOSI`** |
| 33 | `PC4` | *(unconnected)* | **`SD_SNSS`** |
| 34 | `PC5` | *(unconnected)* | **`SD_CD`** |
| 35 | `PB0` | `A16` | **`LED1`** |
| 36 | `PB1` | `A17` | **`LED2`** |
| 38 | `PE8` | `A7` | **`A8`** |
| 39 | `PE9` | `A8` | **`A9`** |
| 40 | `VDDK` | `A9` | **`VDDK`** |
| 41 | `VSS_3` | `A10` | **`GND`** |
| 46 | `VDD_MAIN` | `A15` | **`VCC33`** |
| 47 | `VSS_4` | `!REG` | **`GND`** |
| 48 | `VDD18` | `IORD` | **`VDD18`** |
| 49 | `PB10` | `GND` | **`A14`** |
| 50 | `PB11` | `VCC33` | **`A15`** |
| 51 | `PD8` | `IOWR` | **`D8`** |
| 52 | `PB12` | `RESET` | **`A16`** |
| 53 | `PB13` | `!WAIT` | **`A17`** |
| 54 | `PB14` | `WP/!IOCS16` | **`A18`** |
| 55 | `PB15` | `D8` | **`A19`** |
| 63 | `PC6` | *(unconnected)* | **`CE1`** |
| 64 | `PC7` | `LED2` | **`OE`** |
| 65 | `PC8` | `LED1` | **`!WE`** |
| 66 | `PC9` | `SD_SNSS` | **`CE2`** |
| 67 | `PA8` | *(unconnected)* | **`!WAIT`** |
| 68 | `PA9` | `USART_TX` | **`READY`** |
| 69 | `PA10` | `USART_RX` | **`WP/!IOCS16`** |
| 73 | `PE10` | *(unconnected)* | **`A10`** |
| 74 | `PE11` | `GND` | **`A11`** |
| 77 | `PA15` | `SD_CD` | **`RESET`** |
| 78 | `PC10` | `SD_CLK` | **`!REG`** |
| 79 | `PC11` | `SD_MISO` | **`IORD`** |
| 80 | `PC12` | `SD_MOSI` | **`IOWR`** |
| 89 | `PB3` | `A19` | **`A7`** |
| 92 | `PB6` | `CE1` | **`USART_TX`** |
| 93 | `PB7` | `OE` | **`USART_RX`** |
| 95 | `PB8` | `!WE` | **`A12`** |
| 96 | `PB9` | `CE2` | **`A13`** |


## Already correct

| Pad | Pin | Net |
|-----|-----|-----|
| 1 | `PE2` | `A2` |
| 2 | `PE3` | `A3` |
| 3 | `PE4` | `A4` |
| 4 | `PE5` | `A5` |
| 5 | `PE6` | `A6` |
| 6 | `VBAT` | `VCC33` |
| 10 | `VSS_1` | `GND` |
| 11 | `VDD_1` | `VCC33` |
| 14 | `NRST` | `NRST` |
| 15 | `PC0` | `LED_CLK` |
| 16 | `PC1` | `LED_DO` |
| 19 | `VSSA` | `GND` |
| 20 | `VREF-` | `GND` |
| 21 | `VREF+` | `VCC33` |
| 22 | `VDDA` | `VCC33` |
| 29 | `PA4` | `AUD_MIXED` |
| 56 | `PD9` | `D9` |
| 57 | `PD10` | `D10` |
| 58 | `PD11` | `D11` |
| 59 | `PD12` | `D12` |
| 60 | `PD13` | `D13` |
| 61 | `PD14` | `D14` |
| 62 | `PD15` | `D15` |
| 70 | `PA11` | `USB1_DM` |
| 71 | `PA12` | `USB1_DP` |
| 72 | `PA13` | `SWDIO` |
| 75 | `VDD_3` | `VCC33` |
| 76 | `PA14` | `SWCLK` |
| 81 | `PD0` | `D0` |
| 82 | `PD1` | `D1` |
| 83 | `PD2` | `D2` |
| 84 | `PD3` | `D3` |
| 85 | `PD4` | `D4` |
| 86 | `PD5` | `D5` |
| 87 | `PD6` | `D6` |
| 88 | `PD7` | `D7` |
| 90 | `PB4` | `A20` |
| 91 | `PB5` | `A21` |
| 94 | `PE12-BOOT0` | `BOOT0` |
| 97 | `PE0` | `A0` |
| 98 | `PE1` | `A1` |
| 99 | `VSS_5` | `GND` |
| 100 | `VDD_4` | `VCC33` |


## Also required (not visible in the netlist diff)

1. **Delete the APS6404L SPI RAM** (`SPI_FLASH-X25XX`) and its decoupling. The
   `RAM_SNSS` / `RAM_CLK` / `RAM_MISO` / `RAM_MOSI` nets disappear with it — the SD
   card takes over SPI1 on PA5/PA6/PA7.
2. **New `VDD18` rail** (pad 48): 0.1 uF || 2.2 uF to GND. Internally generated, do
   not drive it externally.
3. **`VDDK` becomes its own net** (pad 40): 0.1 uF || 2.2 uF to GND. It was PE9/A9
   before, so this decoupling does not exist on the board yet.
4. **Main VDD bulk** on pad 46 (`VDD_MAIN`) must be 0.1 uF || >=10 uF, and the total
   VDD capacitance must exceed VDDK + VDD18 combined.
5. **Pads 27/28 (`VSS_2`/`VDD_2`) are unconnected** — this is pre-existing, carried
   over from the STM32 footprint. All same-name power pins must be tied together.
6. **BVD2 audio on PA4 is not 5V tolerant** — AC-couple it into the mixer.
7. **Check no 5V bus signal lands on a non-FT pin**: PA0-PA7, PA11, PA12, PB0, PB1,
   PB6, PB7, PC0-PC5, PC13-PC15.

