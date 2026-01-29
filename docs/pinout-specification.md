# TFPCMCIA Pin Specification
**CH32V303VCT6 PCMCIA Card - Complete Pin Assignment**

Date: 2026-01-29
Hardware Version: 1.0
Total GPIO Pins: 80 (5 ports × 16 pins)
Pins Used: 65 (81% utilization)
Pins Available: 15 (19% expansion capacity)

---

## GPIOA - SPI, Audio, Debug, Status (13/16 used)

| Pin | Signal | Direction | Function | Configuration | Notes |
|-----|--------|-----------|----------|---------------|-------|
| PA0 | STATUS_LED | Output | Status indicator LED | Push-pull, 50MHz | Optional, active HIGH |
| PA1 | SPIRAM_CS | Output | SPI RAM chip select | Push-pull, 50MHz | Manual GPIO, active LOW |
| PA2 | **READY** | **Output** | **PCMCIA READY signal** | **Push-pull, 50MHz** | **Active HIGH when ready** |
| PA3 | - | - | **AVAILABLE** | - | - |
| PA4 | DAC_OUT1/BVD2 | Output | Audio to Amiga | Analog | 12-bit DAC, connects to PCMCIA BVD2 |
| PA5 | SPI1_SCK | Output | SPI RAM clock | Alternate function | 72 MHz max |
| PA6 | SPI1_MISO | Input | SPI RAM data in | Alternate function | Floating |
| PA7 | SPI1_MOSI | Output | SPI RAM data out | Alternate function | 72 MHz max |
| PA8 | BOOTLOADER | Input | Programming mode detect | Pull-down | Active HIGH enters programming |
| PA9 | - | - | **AVAILABLE** | - | - |
| PA10 | USART1_RX | Input | Debug UART receive | Alternate function | **RESERVED** |
| PA11 | USART1_TX | Output | Debug UART transmit | Alternate function | **RESERVED** |
| PA12 | - | - | **AVAILABLE** | - | - |
| PA13 | SWDIO | I/O | SWD debug data | Alternate function | **RESERVED** |
| PA14 | SWCLK | Input | SWD debug clock | Alternate function | **RESERVED** |
| PA15 | SD_CD | Input | SD card detect | Pull-up | Active LOW when card inserted |

**Interface Summary:**
- **SPI1 (SPI RAM):** PA1 (CS), PA5 (SCK), PA6 (MISO), PA7 (MOSI) @ 72 MHz max
- **Audio:** PA4 (DAC → BVD2)
- **Debug:** PA10-PA11 (UART), PA13-PA14 (SWD)
- **Status:** PA0 (LED), PA2 (READY), PA8 (Boot mode), PA15 (SD detect)
- **Available:** PA3, PA9, PA12

---

## GPIOB - Address, Control Signals (16/16 used) ⚠️ **FULLY ALLOCATED**

| Pin | Signal | Direction | Function | Configuration | Notes |
|-----|--------|-----------|----------|---------------|-------|
| PB0 | A16 | Input | Address bit 16 | Floating | |
| PB1 | A17 | Input | Address bit 17 | Floating | |
| PB2 | A18 | Input | Address bit 18 | Floating | |
| PB3 | A19 | Input | Address bit 19 | Floating | |
| PB4 | A20 | Input | Address bit 20 | Floating | |
| PB5 | A21 | Input | Address bit 21 | Floating | 22-bit total (4MB) |
| PB6 | CE1/UDS | Input | Upper Data Strobe | Pull-up | **EXTI6 interrupt**, active LOW |
| PB7 | OE | Input | Memory Output Enable | Pull-up | Memory read cycle, active LOW |
| PB8 | WE | Input | Memory Write Enable | Pull-up | Memory write cycle, active LOW |
| PB9 | CE2/LDS | Input | Lower Data Strobe | Pull-up | **EXTI9 interrupt**, active LOW |
| PB10 | REG | Input | Attribute memory select | Pull-up | 0=Common, 1=Attribute |
| PB11 | IOR | Input | I/O Read strobe | Pull-up | I/O read cycle, active LOW |
| PB12 | IOW | Input | I/O Write strobe | Pull-up | I/O write cycle, active LOW |
| PB13 | RESET | Input | Card reset | Pull-up | Active LOW |
| PB14 | WAIT | Output | Wait state control | Push-pull, 50MHz | LOW=insert waits, HIGH=no wait |
| PB15 | IOIS16 | Output | 16-bit I/O indicator | Push-pull, 50MHz | Optional use |

**Interface Summary:**
- **Address High:** PB0-PB5 (A16-A21)
- **Memory Control:** PB6 (UDS), PB7 (OE), PB8 (WE), PB9 (LDS), PB10 (REG)
- **I/O Control:** PB11 (IOR), PB12 (IOW)
- **System Control:** PB13 (RESET), PB14 (WAIT), PB15 (IOIS16)
- **Interrupts:** EXTI6 (PB6/UDS), EXTI9 (PB9/LDS) → both trigger PCMCIA_Handler

**Control Signal Groups:**
1. **Memory Access:** OE (read) OR WE (write)
2. **I/O Access:** IOR (read) OR IOW (write)
3. **Byte/Word Select:** UDS+LDS (word), UDS only (upper byte), LDS only (lower byte)

---

## GPIOC - SD Card SPI (4/16 used)

| Pin | Signal | Direction | Function | Configuration | Notes |
|-----|--------|-----------|----------|---------------|-------|
| PC0 | - | - | **AVAILABLE** | - | - |
| PC1 | - | - | **AVAILABLE** | - | - |
| PC2 | - | - | **AVAILABLE** | - | - |
| PC3 | - | - | **AVAILABLE** | - | - |
| PC4 | - | - | **AVAILABLE** | - | - |
| PC5 | - | - | **AVAILABLE** | - | - |
| PC6 | - | - | **AVAILABLE** | - | - |
| PC7 | - | - | **AVAILABLE** | - | - |
| PC8 | - | - | **AVAILABLE** | - | - |
| PC9 | SD_CS | Output | SD card chip select | Push-pull, 50MHz | Manual GPIO, active LOW |
| PC10 | SPI3_SCK | Output | SD card SPI clock | Alternate function | 36 MHz max |
| PC11 | SPI3_MISO | Input | SD card SPI data in | Alternate function | Floating |
| PC12 | SPI3_MOSI | Output | SD card SPI data out | Alternate function | 36 MHz max |
| PC13 | - | - | **AVAILABLE** | - | - |
| PC14 | - | - | **AVAILABLE** | - | - |
| PC15 | - | - | **AVAILABLE** | - | - |

**Interface Summary:**
- **SPI3 (SD Card):** PC9 (CS), PC10 (SCK), PC11 (MISO), PC12 (MOSI) @ 36 MHz max
- **Available:** PC0-PC8, PC13-PC15 (12 pins for future expansion)

---

## GPIOD - Data Bus (16/16 used) ⚠️ **FULLY ALLOCATED**

| Pin | Signal | Direction | Function | Configuration | Notes |
|-----|--------|-----------|----------|---------------|-------|
| PD0 | D0 | I/O | Data bit 0 | Bidirectional | Start as input floating |
| PD1 | D1 | I/O | Data bit 1 | Bidirectional | Switch to output on read |
| PD2 | D2 | I/O | Data bit 2 | Bidirectional | |
| PD3 | D3 | I/O | Data bit 3 | Bidirectional | |
| PD4 | D4 | I/O | Data bit 4 | Bidirectional | |
| PD5 | D5 | I/O | Data bit 5 | Bidirectional | |
| PD6 | D6 | I/O | Data bit 6 | Bidirectional | |
| PD7 | D7 | I/O | Data bit 7 | Bidirectional | |
| PD8 | D8 | I/O | Data bit 8 | Bidirectional | |
| PD9 | D9 | I/O | Data bit 9 | Bidirectional | |
| PD10 | D10 | I/O | Data bit 10 | Bidirectional | |
| PD11 | D11 | I/O | Data bit 11 | Bidirectional | |
| PD12 | D12 | I/O | Data bit 12 | Bidirectional | |
| PD13 | D13 | I/O | Data bit 13 | Bidirectional | |
| PD14 | D14 | I/O | Data bit 14 | Bidirectional | |
| PD15 | D15 | I/O | Data bit 15 | Bidirectional | |

**Interface Summary:**
- **Data Bus:** Full 16-bit on single port for atomic operations
- **Access:** Read via `GPIOD->INDR`, write via `GPIOD->OUTDR`
- **Mode Control:** `CFGLR` (D0-D7), `CFGHR` (D8-D15)
- **Configuration Values:**
  - Input: `0x44444444` (floating input)
  - Output: `0x33333333` (push-pull, 50MHz)

---

## GPIOE - Address Bus Low (16/16 used) ⚠️ **FULLY ALLOCATED**

| Pin | Signal | Direction | Function | Configuration | Notes |
|-----|--------|-----------|----------|---------------|-------|
| PE0 | A0 | Input | Address bit 0 | Floating | |
| PE1 | A1 | Input | Address bit 1 | Floating | |
| PE2 | A2 | Input | Address bit 2 | Floating | |
| PE3 | A3 | Input | Address bit 3 | Floating | |
| PE4 | A4 | Input | Address bit 4 | Floating | |
| PE5 | A5 | Input | Address bit 5 | Floating | |
| PE6 | A6 | Input | Address bit 6 | Floating | |
| PE7 | A7 | Input | Address bit 7 | Floating | |
| PE8 | A8 | Input | Address bit 8 | Floating | |
| PE9 | A9 | Input | Address bit 9 | Floating | |
| PE10 | A10 | Input | Address bit 10 | Floating | |
| PE11 | A11 | Input | Address bit 11 | Floating | |
| PE12 | A12 | Input | Address bit 12 | Floating | |
| PE13 | A13 | Input | Address bit 13 | Floating | |
| PE14 | A14 | Input | Address bit 14 | Floating | |
| PE15 | A15 | Input | Address bit 15 | Floating | |

**Interface Summary:**
- **Address Low:** PE0-PE15 (A0-A15)
- **Combined with GPIOB[0-5]:** 22-bit address (A0-A21) = 4MB address space
- **Address Read:** `addr = GPIOE->INDR | ((GPIOB->INDR & 0x3F) << 16)`

---

## Pin Usage Statistics

| Port | Total Pins | Used | Available | Utilization |
|------|------------|------|-----------|-------------|
| GPIOA | 16 | 13 | 3 | 81% |
| GPIOB | 16 | 16 | 0 | **100%** ✓ |
| GPIOC | 16 | 4 | 12 | 25% |
| GPIOD | 16 | 16 | 0 | **100%** ✓ |
| GPIOE | 16 | 16 | 0 | **100%** ✓ |
| **Total** | **80** | **65** | **15** | **81%** |

---

## Functional Pin Grouping

### Address Bus (22 bits)
- **A0-A15:** GPIOE[0-15] (lower 16 bits)
- **A16-A21:** GPIOB[0-5] (upper 6 bits)
- **Address Space:** 0x000000 - 0x3FFFFF (4 MB)

### Data Bus (16 bits)
- **D0-D15:** GPIOD[0-15] (full 16-bit data)
- **Advantage:** Single port = atomic 16-bit read/write

### Memory Control Signals
- **CE1/UDS:** GPIOB[6] - Upper Data Strobe, EXTI6 interrupt
- **CE2/LDS:** GPIOB[9] - Lower Data Strobe, EXTI9 interrupt
- **OE:** GPIOB[7] - Memory read cycle
- **WE:** GPIOB[8] - Memory write cycle
- **REG:** GPIOB[10] - Attribute memory select

### I/O Control Signals
- **IOR:** GPIOB[11] - I/O read cycle
- **IOW:** GPIOB[12] - I/O write cycle
- **IOIS16:** GPIOB[15] - 16-bit I/O indicator

### System Control
- **RESET:** GPIOB[13] - Card reset input
- **WAIT:** GPIOB[14] - Wait state output (cycle extension)
- **READY:** GPIOA[2] - Card ready output (operational status)

### SPI1 Interface (SPI RAM @ 72 MHz)
- **CS:** GPIOA[1] (manual control)
- **SCK:** GPIOA[5]
- **MISO:** GPIOA[6]
- **MOSI:** GPIOA[7]
- **Bus:** APB2 (144 MHz ÷ 2 = 72 MHz max)

### SPI3 Interface (SD Card @ 36 MHz)
- **CS:** GPIOC[9] (manual control)
- **SCK:** GPIOC[10]
- **MISO:** GPIOC[11]
- **MOSI:** GPIOC[12]
- **Bus:** APB1 (72 MHz ÷ 2 = 36 MHz max)

### Audio Interface
- **DAC_OUT1:** GPIOA[4] - 12-bit DAC output to PCMCIA BVD2 pin

### Debug/Programming
- **UART:** GPIOA[10] (RX), GPIOA[11] (TX)
- **SWD:** GPIOA[13] (SWDIO), GPIOA[14] (SWCLK)
- **Bootloader:** GPIOA[8] - Programming mode detect

### Status Indicators
- **Status LED:** GPIOA[0] - General status indicator
- **SD Detect:** GPIOA[15] - SD card presence (LOW = inserted)
- **READY:** GPIOA[2] - Card operational status (HIGH = ready)

---

## Available Pins for Future Expansion

### GPIOA (3 pins)
- PA3
- PA9
- PA12

### GPIOC (12 pins)
- PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8
- PC13, PC14, PC15

**Total Available:** 15 pins

**Potential Future Uses:**
- Additional I/O ports
- Hardware flow control signals
- Additional status LEDs
- External interrupt inputs
- PWM outputs
- Additional SPI/I2C interfaces

---

## Pin Configuration Register Values

### Quick Reference

**Input Floating (Address/Data read):**
```c
0x44444444  // Each nibble = 0x4 (floating input)
```

**Output Push-Pull 50MHz:**
```c
0x33333333  // Each nibble = 0x3 (push-pull, 50MHz)
```

**Input Pull-Up (Control signals):**
```c
GPIO_Mode_IPU  // Use HAL functions for non-contiguous pins
```

**Analog (DAC):**
```c
GPIO_Mode_AIN  // Analog mode
```

**Alternate Function Push-Pull:**
```c
GPIO_Mode_AF_PP  // For SPI SCK/MOSI pins
```

---

## PCMCIA Connector Mapping

### Key PCMCIA Pins

| PCMCIA Pin | Signal | CH32 Pin | Direction | Notes |
|------------|--------|----------|-----------|-------|
| 16 | READY | PA2 | Output | Active HIGH when ready |
| 36 | CD1 | - | - | Hard-wired to GND on PCB |
| 43 | OE | PB7 | Input | Memory read enable |
| 44 | WE | PB8 | Input | Memory write enable |
| 59 | WAIT | PB14 | Output | Active LOW inserts waits |
| 60 | IOIS16 | PB15 | Output | 16-bit I/O indicator |
| 61 | REG | PB10 | Input | Attribute memory select |
| 62 | BVD2 | PA4 | Output | Audio output via DAC |
| 63 | CD2 | - | - | Hard-wired to GND on PCB |
| 7,39 | CE1/UDS | PB6 | Input | Upper data strobe |
| 24,56 | CE2/LDS | PB9 | Input | Lower data strobe |
| Various | A0-A21 | PE0-15, PB0-5 | Input | 22-bit address bus |
| Various | D0-D15 | PD0-15 | I/O | 16-bit data bus |

---

## Interrupt Configuration

### EXTI Lines

| EXTI Line | GPIO Pin | Signal | Trigger | Priority | Handler |
|-----------|----------|--------|---------|----------|---------|
| EXTI6 | PB6 | CE1/UDS | Falling edge | 0 (highest) | PCMCIA_Handler |
| EXTI9 | PB9 | CE2/LDS | Falling edge | 0 (highest) | PCMCIA_Handler |

**Notes:**
- Both interrupts share IRQ handler: `EXTI9_6_IRQn`
- Use WCH fast interrupt mode: `SetVTFIRQ()`
- Handler clears both flags: `EXTI->INTFR = EXTI_Line6 | EXTI_Line9`
- Either UDS or LDS falling triggers the handler

---

## Timing Critical Paths

### Bus Cycle Timing (Target: <120ns)

1. **Interrupt Latency:** ~55ns (WCH-Interrupt-fast mode)
2. **Address Read:** ~14ns (2 GPIO port reads)
3. **Memory Decode:** ~20ns (if/else logic)
4. **Data Output:** ~21ns (GPIO mode switch + write)
5. **Total:** ~110ns ✓ (10ns margin)

### Critical Registers

**Direct access for speed (no HAL):**
- `GPIOE->INDR` - Address low (A0-A15)
- `GPIOB->INDR` - Address high (A16-A21) + control signals
- `GPIOD->INDR` - Data bus read
- `GPIOD->OUTDR` - Data bus write
- `GPIOD->CFGLR/CFGHR` - Data bus direction
- `EXTI->INTFR` - Interrupt flag clear

---

## Design Notes

### Address Space (Card Perspective)
```
0x000000 - 0x03FFFF: ROM (256KB, Flash-based)
0x040000 - 0x1FFFFF: SPI RAM (1.75MB, optional)
0x1F0000 - 0x1F00FF: I/O Ports (256 bytes)
0x200000 - 0x3FFFFF: Unmapped
Attribute Memory: 512 bytes (REG=1)
```

### Amiga Memory Map
```
$600000 - $63FFFF: ROM
$640000 - $7FFFFF: SPI RAM
$9F0000 - $9F00FF: I/O Ports
$A00000 - $A001FF: Attribute Memory
```

### Signal Polarity Reference

**Active LOW:**
- All PCMCIA control inputs (OE, WE, IOR, IOW, UDS, LDS, RESET)
- Chip selects (SPIRAM_CS, SD_CS)
- SD card detect (SD_CD)
- WAIT output

**Active HIGH:**
- READY output
- Status LED
- Data bus (when driven)
- IOIS16 (when asserted)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-29 | Initial pinout specification |
|  |  | - Added IOR/IOW on PB11/PB12 |
|  |  | - Added READY on PA2 |
|  |  | - Added SD_CD on PA15 |

---

**End of Pin Specification**
