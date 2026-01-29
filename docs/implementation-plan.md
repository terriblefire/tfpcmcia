# Amiga PCMCIA Card Firmware Implementation Plan

## Overview

Adapt the CH32V30x MSX cartridge firmware to create an Amiga PCMCIA card with:
- **22-bit address bus** (A0-A21) = 4MB address space
- **16-bit data bus** (D0-D15)
- **256KB ROM** at base address (loaded from SD card)
- **Attribute memory** for PCMCIA card information structure
- **I/O ports** for SD card control
- **Optional SPI RAM** for extended storage
- **Standard PCMCIA timing** (120ns cycles)

**Approach:** GPIO interrupt-driven (proven MSX architecture), NOT FSMC (due to address line limitations and missing driver implementation).

---

## 1. Complete Pin Assignment Summary

### Quick Reference Table

| Pin | Signal | Direction | Function | Notes |
|-----|--------|-----------|----------|-------|
| **GPIOA (Debug/Audio)** |
| PA0 | STATUS_LED | Output | Status indicator LED | Optional |
| PA1 | SPIRAM_CS | Output | SPI RAM chip select | Manual GPIO control |
| PA2 | **READY** | **Output** | **Card ready signal** | **Push-pull, active HIGH when ready** |
| PA3 | - | - | Available | 1 pin free |
| PA4 | DAC_OUT1 / BVD2 | Output | Audio output to Amiga | Analog, connects to PCMCIA BVD2 |
| PA5 | SPI1_SCK | Output | SPI RAM clock | Alternate function, 72 MHz max |
| PA6 | SPI1_MISO | Input | SPI RAM data in | Alternate function |
| PA7 | SPI1_MOSI | Output | SPI RAM data out | Alternate function |
| PA8 | BOOTLOADER | Input | Programming mode detect | Pull-down |
| PA9 | - | - | Available | 1 pin free |
| PA10 | USART1_RX | Input | Debug UART receive | **RESERVED** |
| PA11 | USART1_TX | Output | Debug UART transmit | **RESERVED** |
| PA12 | - | - | Available | 1 pin free |
| PA13 | SWDIO | I/O | SWD debug data | **RESERVED** |
| PA14 | SWCLK | Input | SWD debug clock | **RESERVED** |
| PA15 | SD_CD | Input | SD card detect | Pull-up, LOW when card inserted |
| **GPIOB (Address + Control)** |
| PB0 | A16 | Input | Address bit 16 | Floating |
| PB1 | A17 | Input | Address bit 17 | Floating |
| PB2 | A18 | Input | Address bit 18 | Floating |
| PB3 | A19 | Input | Address bit 19 | Floating |
| PB4 | A20 | Input | Address bit 20 | Floating |
| PB5 | A21 | Input | Address bit 21 | Floating |
| PB6 | CE1 / UDS | Input | Upper Data Strobe | **EXTI6 interrupt**, pull-up |
| PB7 | OE | Input | Output Enable (read) | Pull-up |
| PB8 | WE | Input | Write Enable | Pull-up |
| PB9 | CE2 / LDS | Input | Lower Data Strobe | **EXTI9 interrupt**, pull-up |
| PB10 | REG | Input | Attribute space select | Pull-up |
| PB11 | IOR | Input | I/O Read strobe | Pull-up, active LOW |
| PB12 | IOW | Input | I/O Write strobe | Pull-up, active LOW |
| PB13 | RESET | Input | Card reset | Pull-up |
| PB14 | WAIT | Output | Wait state control | Push-pull, active LOW inserts wait states |
| PB15 | IOIS16 | Output | 16-bit I/O indicator | Push-pull, or spare |
| **GPIOC (SPI + Expansion)** |
| PC0-PC8 | - | - | Available | 9 pins free |
| PC9 | SD_CS | Output | SD card chip select | Push-pull, active LOW |
| PC10 | SPI3_SCK | Output | SD card SPI clock | Alternate function |
| PC11 | SPI3_MISO | Input | SD card SPI data in | Floating |
| PC12 | SPI3_MOSI | Output | SD card SPI data out | Alternate function |
| PC13-PC15 | - | - | Available | 3 pins free |
| **GPIOD (Data Bus)** |
| PD0 | D0 | I/O | Data bit 0 | Bidirectional |
| PD1 | D1 | I/O | Data bit 1 | Bidirectional |
| PD2 | D2 | I/O | Data bit 2 | Bidirectional |
| PD3 | D3 | I/O | Data bit 3 | Bidirectional |
| PD4 | D4 | I/O | Data bit 4 | Bidirectional |
| PD5 | D5 | I/O | Data bit 5 | Bidirectional |
| PD6 | D6 | I/O | Data bit 6 | Bidirectional |
| PD7 | D7 | I/O | Data bit 7 | Bidirectional |
| PD8 | D8 | I/O | Data bit 8 | Bidirectional |
| PD9 | D9 | I/O | Data bit 9 | Bidirectional |
| PD10 | D10 | I/O | Data bit 10 | Bidirectional |
| PD11 | D11 | I/O | Data bit 11 | Bidirectional |
| PD12 | D12 | I/O | Data bit 12 | Bidirectional |
| PD13 | D13 | I/O | Data bit 13 | Bidirectional |
| PD14 | D14 | I/O | Data bit 14 | Bidirectional |
| PD15 | D15 | I/O | Data bit 15 | Bidirectional |
| **GPIOE (Address Bus)** |
| PE0 | A0 | Input | Address bit 0 | Floating |
| PE1 | A1 | Input | Address bit 1 | Floating |
| PE2 | A2 | Input | Address bit 2 | Floating |
| PE3 | A3 | Input | Address bit 3 | Floating |
| PE4 | A4 | Input | Address bit 4 | Floating |
| PE5 | A5 | Input | Address bit 5 | Floating |
| PE6 | A6 | Input | Address bit 6 | Floating |
| PE7 | A7 | Input | Address bit 7 | Floating |
| PE8 | A8 | Input | Address bit 8 | Floating |
| PE9 | A9 | Input | Address bit 9 | Floating |
| PE10 | A10 | Input | Address bit 10 | Floating |
| PE11 | A11 | Input | Address bit 11 | Floating |
| PE12 | A12 | Input | Address bit 12 | Floating |
| PE13 | A13 | Input | Address bit 13 | Floating |
| PE14 | A14 | Input | Address bit 14 | Floating |
| PE15 | A15 | Input | Address bit 15 | Floating |

### Pin Usage Statistics

| Port | Total | Used | Function Groups | Available |
|------|-------|------|-----------------|-----------|
| GPIOA | 16 | 12 | SPI1(4), DAC(1), UART(2), SWD(2), Boot(1), SD_CD(1), READY(1) | 4 pins |
| GPIOB | 16 | 16 | Address A16-A21(6), Control(10) | 0 pins |
| GPIOC | 16 | 4 | SPI3(4) | 12 pins |
| GPIOD | 16 | 16 | Data D0-D15(16) | 0 pins |
| GPIOE | 16 | 16 | Address A0-A15(16) | 0 pins |
| **Total** | **80** | **64** | | **16** |

**Utilization:** 80% (64/80 pins used)

---

## 2. Detailed Pin Allocation by Function

### Data Bus (16 bits) - GPIOD
**All on single port for atomic 16-bit operations:**
- **GPIOD[0-15]**: D0-D15 (full 16-bit data bus)
  - **Advantage:** Single port = atomic read/write via `GPIOD->INDR` / `GPIOD->OUTDR`
  - **Mode switching:** Both `CFGLR` (D0-D7) and `CFGHR` (D8-D15) for direction control

### Address Bus (22 bits) - GPIOE + GPIOB
**Lower 16 bits on GPIOE, upper 6 bits on GPIOB:**
- **GPIOE[0-15]**: A0-A15 (lower address bits)
- **GPIOB[0-5]**: A16-A21 (upper address bits)
  - **Total:** 22 address lines = 4MB address space (0x000000 - 0x3FFFFF)
  - **Reading:** `addr = GPIOE->INDR | ((GPIOB->INDR & 0x3F) << 16)`

### PCMCIA Control Signals - GPIOB[6-15]
**All control signals grouped on upper GPIOB:**
- **PB6**: CE1/UDS (Upper Data Strobe) - **interrupt trigger** (EXTI6), active low
- **PB7**: OE (Output Enable) - memory read cycle, active low
- **PB8**: WE (Write Enable) - memory write cycle, active low
- **PB9**: CE2/LDS (Lower Data Strobe) - **interrupt trigger** (EXTI9), active low
- **PB10**: REG (Register Select) - 0=Common memory, 1=Attribute memory
- **PB11**: IOR (I/O Read) - I/O read cycle, active low
- **PB12**: IOW (I/O Write) - I/O write cycle, active low
- **PB13**: RESET (Reset) - input, active low
- **PB14**: WAIT - **output**, low=insert wait states, high=no wait
- **PB15**: IOIS16 (16-bit I/O indicator) - **output**, or spare

**Memory vs I/O Access:**
- **Memory access**: OE (read) or WE (write) active
- **I/O access**: IOR (read) or IOW (write) active
- These are mutually exclusive - Gayle asserts either memory OR I/O strobes, not both

**Byte/Word Access (via CE1/CE2):**
- 16-bit access: Both UDS (CE1) and LDS (CE2) asserted
- Upper byte only: UDS asserted, LDS inactive
- Lower byte only: LDS asserted, UDS inactive

### SD Card Interface - SPI3 on GPIOC
**Using hardware SPI3 peripheral (APB1 bus):**
- **PC9**: SD_CS (Chip Select, manual GPIO control)
- **PC10**: SPI3_SCK (SPI Clock)
- **PC11**: SPI3_MISO (Master In Slave Out)
- **PC12**: SPI3_MOSI (Master Out Slave In)
  - **SPI Speed:** 36 MHz max (APB1 @ 72MHz / 2 prescaler)
  - **SPI Mode:** Mode 0 (CPOL=0, CPHA=0)
  - **Throughput:** ~4.5 MB/s theoretical (sufficient for SD cards)

### SPI RAM Interface - SPI1 on GPIOA
**Using hardware SPI1 peripheral (APB2 bus) for maximum speed:**
- **PA1**: SPIRAM_CS (Chip Select, manual GPIO control)
- **PA5**: SPI1_SCK (SPI Clock)
- **PA6**: SPI1_MISO (Master In Slave Out)
- **PA7**: SPI1_MOSI (Master Out Slave In)
  - **SPI Speed:** 72 MHz max (APB2 @ 144MHz / 2 prescaler)
  - **SPI Mode:** Mode 0 (CPOL=0, CPHA=0)
  - **Throughput:** ~9 MB/s theoretical (2x faster than SPI3)

**IMPORTANT:** CH32V30x does NOT support QSPI (Quad SPI):
- Only standard SPI available (1-bit MOSI + 1-bit MISO)
- SPI RAM must be standard SPI, not QSPI
- QSPI flash chips can be used in SPI compatibility mode only
- Max speed: ~36-72 MHz in standard SPI mode

### Audio Output & SPI RAM - GPIOA
- **PA1**: SPIRAM_CS (SPI RAM chip select) - manual GPIO control
- **PA4**: DAC_OUT1 (analog audio output, connects to BVD2 pin on PCMCIA connector)
  - **Purpose:** Send analog audio to Amiga via Battery Voltage Detect pin
  - **DAC:** 12-bit, driven by TIM4/DMA for audio samples
- **PA5-PA7**: SPI1 interface (SCK, MISO, MOSI) for SPI RAM @ 72 MHz max

### Debug/Programming/Status - GPIOA
- **PA10**: USART1_RX (debug UART receive) - **RESERVED**
- **PA11**: USART1_TX (debug UART transmit) - **RESERVED**
- **PA13**: SWDIO (SWD debug data) - **RESERVED**
- **PA14**: SWCLK (SWD debug clock) - **RESERVED**
- **PA0**: Status LED (optional, active high)
- **PA2**: READY output (push-pull, active HIGH when card ready)
- **PA8**: Programming/bootloader mode detect (input with pull-down)
- **PA15**: SD card detect (input with pull-up, LOW when card inserted)

### Reserved/Available Pins
- **GPIOA[3, 9, 12]**: 3 pins available
- **GPIOC[0-8, 13-15]**: 12 pins available
- Total available: ~15 pins for future expansion

### Pin Usage Summary
| Port | Used | Function | Available |
|------|------|----------|-----------|
| GPIOA | 13 | SPI1 (4), DAC (1), UART(2), SWD(2), Boot(1), LED(1), SD_CD(1), READY(1) | 3 pins |
| GPIOB | 16 | Address A16-A21 (6), Control (10: UDS, OE, WE, LDS, REG, IOR, IOW, RESET, WAIT, IOIS16) | 0 pins |
| GPIOC | 4 | SPI3 (4) | 12 pins |
| GPIOD | 16 | Data D0-D15 (16) | 0 pins |
| GPIOE | 16 | Address A0-A15 (16) | 0 pins |
| **Total** | **65** | | **15** |

**Total Pins Used:** 65 pins (81% utilization of 80 GPIO pins on CH32V303VCT6)

---

## 2. Memory Architecture

### Address Space Map

**From Card's Perspective (A0-A21):**
```
0x000000 - 0x03FFFF: ROM (256KB from SD card buffer in Flash)
0x040000 - 0x1FFFFF: SPI RAM (1.75MB, optional)
0x1F0000 - 0x1F00FF: I/O Ports (SD card control registers)
0x200000 - 0x3FFFFF: Unmapped (returns 0xFFFF)
Attribute Memory: Separate space selected via REG signal (512 bytes)
```

**From Amiga's Perspective (how Amiga accesses the card):**
```
$600000 - $63FFFF: ROM (card address 0x000000 - 0x03FFFF)
$640000 - $7FFFFF: SPI RAM (card address 0x040000 - 0x1FFFFF)
$9F0000 - $9F00FF: I/O Ports (card address 0x1F0000 - 0x1F00FF)
$A00000 - $A001FF: Attribute Memory (REG=1, 512 bytes)
```

**Note:** The card hardware only sees addresses A0-A21 starting from 0x000000. The Amiga's Gayle chip maps these to the $600000-$9FFFFF range. The card doesn't need to know about this offset.

### ROM Buffer Strategy
**Use Flash-based ROM buffer** (not RAM):
- Program ROM into Flash at `__cart_section_start` (0x08008000)
- Load from SD card during initialization or programming mode
- 256KB Flash available for cartridge section
- Fast access (0 wait states at 144MHz)
- Preserves 32KB SRAM for stack/heap/variables

### Attribute Memory
- 512-byte static array containing PCMCIA CIS (Card Information Structure)
- Includes card identification, voltage, timing specifications
- Standard PCMCIA tuples for memory card type

---

## 3. Interrupt Handler Architecture

### Trigger: CE1/UDS or CE2/LDS Falling Edge
- Configure **EXTI6** on GPIOB[6] (CE1/UDS - Upper Data Strobe)
- Configure **EXTI9** on GPIOB[9] (CE2/LDS - Lower Data Strobe)
- **Both interrupts trigger the same handler** for byte-level or word-level access
- Falling edge = card selected by Amiga (byte or word access)
- Highest priority (0) for minimal latency
- Use `WCH-Interrupt-fast` attribute

**Why dual interrupts?**
- 16-bit word access: Both UDS and LDS assert (either can trigger handler)
- Upper byte access: Only UDS asserts
- Lower byte access: Only LDS asserts
- Handler must check which strobe(s) are active to determine access width

### Pin Bit Masks (for quick reference)
```c
#define UDS_MASK    0x0040  // PB6  - bit 6 (CE1/UDS - Upper Data Strobe)
#define OE_MASK     0x0080  // PB7  - bit 7 (Memory Read)
#define WE_MASK     0x0100  // PB8  - bit 8 (Memory Write)
#define LDS_MASK    0x0200  // PB9  - bit 9 (CE2/LDS - Lower Data Strobe)
#define REG_MASK    0x0400  // PB10 - bit 10 (Attribute Memory Select)
#define IOR_MASK    0x0800  // PB11 - bit 11 (I/O Read)
#define IOW_MASK    0x1000  // PB12 - bit 12 (I/O Write)
#define RESET_MASK  0x2000  // PB13 - bit 13
#define WAIT_MASK   0x4000  // PB14 - bit 14
#define IOIS16_MASK 0x8000  // PB15 - bit 15
#define ADDR_HI_MASK 0x003F // PB[0-5] - bits 0-5 (A16-A21)
```

### Handler Flow (Optimized)
```c
void PCMCIA_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void PCMCIA_Handler(void) {
    // Clear both interrupt flags immediately
    EXTI->INTFR = EXTI_Line6 | EXTI_Line9;

    // Read 22-bit address (early)
    uint32_t addr_low = GPIOE->INDR;              // A0-A15
    uint32_t addr_high = GPIOB->INDR & 0x3F;      // A16-A21 (bits 0-5)
    uint32_t address = addr_low | (addr_high << 16);

    // Read control signals and potential write data
    uint16_t control = GPIOB->INDR;
    uint8_t reg = (control >> 10) & 0x01;         // REG signal (PB10)
    uint16_t write_data = GPIOD->INDR;

    // Wait while either UDS or LDS asserted
    while (((GPIOB->INDR & 0x0040) == 0) || ((GPIOB->INDR & 0x0200) == 0)) {
        control = GPIOB->INDR;  // Re-read control signals

        // MEMORY READ CYCLE: OE asserted (PB7 = bit 7 = 0x0080)
        if ((control & 0x0080) == 0) {
            uint16_t data;

            if (reg) {
                // Attribute memory access
                data = attribute_memory[address & 0x1FF];
            } else if (address < 0x40000) {
                // ROM region (Flash-based)
                data = rom_buffer[address >> 1];
            } else {
                // Unmapped or SPI RAM
                data = 0xFFFF;
            }

            // Drive data bus
            GPIOD->OUTDR = data;
            GPIOD->CFGLR = 0x33333333;  // D0-D7 output
            GPIOD->CFGHR = 0x33333333;  // D8-D15 output

            while (((GPIOB->INDR & 0x0040) == 0) || ((GPIOB->INDR & 0x0200) == 0)) {};

            // Release data bus
            GPIOD->CFGLR = 0x44444444;  // D0-D7 input
            GPIOD->CFGHR = 0x44444444;  // D8-D15 input
            return;
        }

        // MEMORY WRITE CYCLE: WE asserted (PB8 = bit 8 = 0x0100)
        if ((control & 0x0100) == 0) {
            if (reg) {
                write_attribute_register(address, write_data);
            }
            // ROM is read-only, SPI RAM writes handled separately
            return;
        }

        // I/O READ CYCLE: IOR asserted (PB11 = bit 11 = 0x0800)
        if ((control & 0x0800) == 0) {
            uint16_t data = read_io_port(address & 0xFF);

            // Drive data bus
            GPIOD->OUTDR = data;
            GPIOD->CFGLR = 0x33333333;  // D0-D7 output
            GPIOD->CFGHR = 0x33333333;  // D8-D15 output

            while (((GPIOB->INDR & 0x0040) == 0) || ((GPIOB->INDR & 0x0200) == 0)) {};

            // Release data bus
            GPIOD->CFGLR = 0x44444444;  // D0-D7 input
            GPIOD->CFGHR = 0x44444444;  // D8-D15 input
            return;
        }

        // I/O WRITE CYCLE: IOW asserted (PB12 = bit 12 = 0x1000)
        if ((control & 0x1000) == 0) {
            write_io_port(address & 0xFF, write_data);
            return;
        }
    }
}
```

### Byte vs Word Access Handling

The Amiga uses **UDS (Upper Data Strobe)** and **LDS (Lower Data Strobe)** for byte-granular memory access:

**Access Type Detection:**
- **Word access (16-bit):** Both UDS and LDS asserted simultaneously
  - Read/write full 16-bit data on D0-D15
  - Address is word-aligned (A0=0)

- **Upper byte access (8-bit):** Only UDS asserted
  - Read/write upper byte on D8-D15
  - Lower byte (D0-D7) ignored
  - Address bit A0=0

- **Lower byte access (8-bit):** Only LDS asserted
  - Read/write lower byte on D0-D7
  - Upper byte (D8-D15) ignored
  - Address bit A0=1

**Implementation Notes:**
1. The handler can check `IS_WORD_ACCESS()` vs `IS_BYTE_ACCESS()` if needed
2. For read-only ROM, byte vs word access doesn't matter - always return full 16-bit word
3. For write operations (I/O ports, SPI RAM), must handle byte lanes correctly:
   ```c
   if (IS_WRITE_CYCLE()) {
       if (IS_UDS_ACTIVE() && IS_LDS_ACTIVE()) {
           // Word write: use full 16-bit data
           write_word(address, write_data);
       } else if (IS_UDS_ACTIVE()) {
           // Upper byte write: use bits 8-15 only
           write_byte(address, (write_data >> 8) & 0xFF);
       } else if (IS_LDS_ACTIVE()) {
           // Lower byte write: use bits 0-7 only
           write_byte(address, write_data & 0xFF);
       }
   }
   ```

4. ROM reads are simplified - Amiga will mask off unwanted byte:
   ```c
   // ROM always returns full 16-bit word
   // Amiga hardware ignores the byte not selected by UDS/LDS
   data = rom_buffer[address >> 1];
   ```

### Timing Analysis
- Interrupt latency: ~55ns (fast interrupt mode)
- Address read (2 GPIO ports): ~14ns
- Memory decode + fetch: ~20ns
- Data output + mode switch: ~21ns
- **Total: ~110ns** (within 120ns PCMCIA spec with 10ns margin)

---

## 4. Critical Files to Modify

### Core Firmware Files

**1. firmware/User/gpio.c**
- Configure GPIOE[0-15] as input floating (address bits A0-A15)
- Configure GPIOB[0-5] as input floating (address bits A16-A21)
- Extend GPIOD to 16-bit mode (add CFGHR configuration for D8-D15)
- Configure GPIOB[6-10,13] as input pull-up (PCMCIA control inputs, CD1/CD2 hard-wired to GND)
- Configure GPIOB[14] as output push-pull (READY signal)
- Set up DAC output on GPIOA[4] (analog mode for BVD2 audio)
- Set up SPI1 on GPIOA[5-7] (hardware SPI for SPI RAM @ 72 MHz)
- Set up SPI3 on GPIOC[9-12] (hardware SPI for SD card @ 36 MHz)
- Configure EXTI6 and EXTI9 interrupts on GPIOB[6,9] (UDS/LDS triggers)

**2. firmware/User/gpio.h**
- Add PCMCIA signal pin definitions
- Add inline functions for data bus direction control:
  - `data_bus_input()` - switch to high-Z
  - `data_bus_output()` - switch to push-pull
  - `data_bus_write(uint16_t)` - write 16-bit data
  - `data_bus_read()` - read 16-bit data

**3. firmware/User/cart.c**
- Replace all MSX mapper handlers with single `PCMCIA_Handler()`
- Implement memory region decoding logic
- Remove MSX bank switching code
- Add attribute memory read/write functions
- Add I/O port handlers

**4. firmware/User/cart.h**
- Remove MSX `CartType` enum and state structures
- Add PCMCIA configuration defines
- Add memory map constants
- Update function prototypes

**5. firmware/User/main.c**
- Replace `Init_Cart()` with `Init_PCMCIA()`
- Add SD card initialization sequence
- Add ROM loading from SD card to Flash
- Remove MSX-specific startup code
- Add programming mode detection (for flashing new ROM)

### New Files to Create

**6. firmware/User/pcmcia.c**
- PCMCIA attribute memory data (CIS structure)
- Attribute memory access functions
- I/O port read/write implementations
- SD card integration functions
- PCMCIA initialization routine

**7. firmware/User/pcmcia.h**
- PCMCIA constants and memory map
- Control signal bit masks
- I/O port address definitions
- CIS tuple definitions
- Function prototypes

**8. firmware/User/sd_interface.c** (if not already implemented)
- SD card SPI driver
- File system operations (FAT)
- ROM file loading functions

---

## 5. Implementation Steps

### Step 1: GPIO Reconfiguration
- Modify `GPIO_Config()` in gpio.c:
  - Set GPIOE[0-15] to input floating mode (address lines A0-A15)
  - Set GPIOB[0-5] to input floating mode (address lines A16-A21)
  - Configure GPIOD[0-15] for 16-bit bidirectional data (start as input)
  - Set GPIOB[6-13] to input pull-up mode (control inputs: UDS, OE, WE, LDS, REG, IOR, IOW, RESET)
  - Configure GPIOB[14] (WAIT) as output push-pull, set HIGH (no wait states)
  - Configure GPIOB[15] (IOIS16) as output push-pull if used
  - Set up DAC output on GPIOA[4] (analog mode for BVD2 audio)
  - Set up SPI1 pins on GPIOA[5-7] using alternate function mode (SPI RAM @ 72 MHz)
  - Set up SPI3 pins on GPIOC[9-12] using alternate function mode (SD card @ 36 MHz)
  - Set up SD card detect on GPIOA[15] (input pull-up, LOW when card inserted)

### Step 2: Interrupt Handler Migration
- In cart.c, replace MSX handlers with `PCMCIA_Handler()`
- Implement 22-bit address reading (GPIOE + GPIOB)
- Add memory region decode logic
- Implement 16-bit data bus control
- Update interrupt to use EXTI6 (GPIOB[6]) instead of EXTI3

### Step 3: Attribute Memory Implementation
- Create CIS data structure with PCMCIA identification
- Implement `read_attribute_memory()` function
- Add configuration register handling

### Step 4: I/O Port Interface
- Define I/O port address space (0x1F0000-0x1F00FF)
- Implement `read_io_port()` for SD card status/data
- Implement `write_io_port()` for SD card commands

### Step 5: SD Card Integration
- Initialize SPI peripheral
- Mount FAT filesystem
- Load ROM file from SD card into Flash buffer
- Handle ROM programming mode

### Step 6: Initialization Updates
- Create `Init_PCMCIA()` function
- Set EXTI6 to trigger on UDS/CE1 (GPIOB[6])
- Set EXTI9 to trigger on LDS/CE2 (GPIOB[9])
- Both interrupts trigger same `PCMCIA_Handler()`
- Enable fast interrupt mode via `SetVTFIRQ()`
- Set READY signal high (GPIOB[14])

### Step 7: Data Bus Direction Control
- Add inline functions for switching GPIOD direction
- Ensure proper timing for output enable/disable
- Use direct register access for speed

### Step 8: Flash ROM Buffer Setup
- Use existing `__cart_section_start` (0x08008000)
- Implement Flash programming function
- Add ROM update mechanism via programming mode

---

## 6. Key Code Examples

### Complete GPIO Configuration (gpio.c)

```c
void GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};

    // Enable all GPIO clocks + AFIO
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                      RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                      RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO;

    // ===== ADDRESS BUS =====
    // GPIOE[0-15]: A0-A15 (input floating)
    GPIOE->CFGLR = 0x44444444;
    GPIOE->CFGHR = 0x44444444;

    // GPIOB[0-5]: A16-A21 (input floating)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                                  GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ===== DATA BUS =====
    // GPIOD[0-15]: D0-D15 (start as input, switch to output during read cycles)
    GPIOD->CFGLR = 0x44444444;  // D0-D7 input floating
    GPIOD->CFGHR = 0x44444444;  // D8-D15 input floating

    // ===== PCMCIA CONTROL SIGNALS =====
    // GPIOB[6-13]: Control inputs (UDS, OE, WE, LDS, REG, IOR, IOW, RESET)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 |
                                  GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
                                  GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // GPIOB[14]: WAIT output (start HIGH = no wait states)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_14);  // WAIT high (no wait states)

    // ===== READY OUTPUT (GPIOA) =====
    // PA2: READY (card ready signal)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_2);  // READY high (card is ready)

    // ===== DAC FOR AUDIO OUTPUT (GPIOA) =====
    // PA4: DAC_OUT1 (analog output to BVD2)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  // Analog input mode (DAC uses this)
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ===== SPI1 FOR SPI RAM (GPIOA) =====
    // PA5, PA7: SCK, MOSI (alternate function push-pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA6: MISO (input floating)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA1: SPI RAM CS (manual GPIO control)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_1);  // CS high (deselected)

    // ===== SPI3 FOR SD CARD (GPIOC) =====
    // PC10, PC12: SCK, MOSI (alternate function push-pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // PC11: MISO (input floating)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // PC9: CS (manual GPIO control)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_9);  // CS high (deselected)

    // ===== SD CARD DETECT (GPIOA) =====
    // PA15: SD_CD (card detect, active LOW)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // Input with pull-up
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ===== INTERRUPT CONFIGURATION =====
    // EXTI6 on GPIOB[6] (CE1/UDS) - falling edge trigger
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource6);
    EXTI_InitStructure.EXTI_Line = EXTI_Line6;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // EXTI9 on GPIOB[9] (CE2/LDS) - falling edge trigger
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource9);
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
}
```

### PCMCIA Initialization (cart.c or pcmcia.c)

```c
void Init_PCMCIA(void) {
    // Set EXTI6 and EXTI9 interrupts to highest priority
    // Both EXTI6 and EXTI9 share EXTI9_6_IRQn handler
    NVIC_SetPriority(EXTI9_6_IRQn, 0);

    // Enable fast interrupt handling (WCH-specific)
    // Both EXTI6 and EXTI9 will trigger PCMCIA_Handler
    SetVTFIRQ((u32)PCMCIA_Handler, EXTI9_6_IRQn, 0, ENABLE);

    // Enable EXTI9_6 interrupt (handles both EXTI6 and EXTI9)
    NVIC_EnableIRQ(EXTI9_6_IRQn);

    // Initialize WAIT signal HIGH (no wait states)
    GPIO_SetBits(GPIOB, GPIO_Pin_14);  // WAIT high (no wait states)
}
```

### Helper Macros (pcmcia.h)

```c
// Pin bit masks for GPIOB control signals
#define PCMCIA_UDS     0x0040  // PB6  - bit 6 (CE1/Upper Data Strobe)
#define PCMCIA_OE      0x0080  // PB7  - bit 7 (Memory Read)
#define PCMCIA_WE      0x0100  // PB8  - bit 8 (Memory Write)
#define PCMCIA_LDS     0x0200  // PB9  - bit 9 (CE2/Lower Data Strobe)
#define PCMCIA_REG     0x0400  // PB10 - bit 10 (Attribute Memory)
#define PCMCIA_IOR     0x0800  // PB11 - bit 11 (I/O Read)
#define PCMCIA_IOW     0x1000  // PB12 - bit 12 (I/O Write)
#define PCMCIA_RESET   0x2000  // PB13 - bit 13
#define PCMCIA_WAIT    0x4000  // PB14 - bit 14
#define PCMCIA_IOIS16  0x8000  // PB15 - bit 15

// Address extraction macros
#define ADDR_LOW()     (GPIOE->INDR)
#define ADDR_HIGH()    (GPIOB->INDR & 0x3F)
#define ADDR_FULL()    (ADDR_LOW() | (ADDR_HIGH() << 16))

// Control signal checks
#define IS_MEM_READ()     ((GPIOB->INDR & PCMCIA_OE) == 0)
#define IS_MEM_WRITE()    ((GPIOB->INDR & PCMCIA_WE) == 0)
#define IS_IO_READ()      ((GPIOB->INDR & PCMCIA_IOR) == 0)
#define IS_IO_WRITE()     ((GPIOB->INDR & PCMCIA_IOW) == 0)
#define IS_ATTR_MEM()     ((GPIOB->INDR & PCMCIA_REG) != 0)
#define IS_UDS_ACTIVE()   ((GPIOB->INDR & PCMCIA_UDS) == 0)
#define IS_LDS_ACTIVE()   ((GPIOB->INDR & PCMCIA_LDS) == 0)

// Access width detection
#define IS_WORD_ACCESS()  (IS_UDS_ACTIVE() && IS_LDS_ACTIVE())
#define IS_BYTE_ACCESS()  (IS_UDS_ACTIVE() ^ IS_LDS_ACTIVE())

// Data bus control
#define DATA_BUS_OUTPUT() do { \
    GPIOD->CFGLR = 0x33333333; \
    GPIOD->CFGHR = 0x33333333; \
} while(0)

#define DATA_BUS_INPUT() do { \
    GPIOD->CFGLR = 0x44444444; \
    GPIOD->CFGHR = 0x44444444; \
} while(0)

#define DATA_BUS_WRITE(data) (GPIOD->OUTDR = (data))
#define DATA_BUS_READ()      (GPIOD->INDR)

// WAIT signal control (active LOW inserts wait states)
#define WAIT_DISABLE()   GPIO_SetBits(GPIOB, GPIO_Pin_14)    // High = no wait
#define WAIT_ENABLE()    GPIO_ResetBits(GPIOB, GPIO_Pin_14)  // Low = insert wait

// READY signal control (active HIGH when card is ready)
#define READY_SET()      GPIO_SetBits(GPIOA, GPIO_Pin_2)      // High = ready
#define READY_CLEAR()    GPIO_ResetBits(GPIOA, GPIO_Pin_2)    // Low = busy
```

---

## 7. SD Card Integration Details

### SPI Configuration

**SPI1 for SPI RAM (APB2 bus):**
- Pins: GPIOA[1,5-7] (CS, SCK, MISO, MOSI)
- Clock speed: 72 MHz max (APB2 @ 144MHz / 2 prescaler)
- Mode: SPI Mode 0 (CPOL=0, CPHA=0)
- CS (PA1) controlled manually via GPIO
- Throughput: ~9 MB/s theoretical

**SPI3 for SD Card (APB1 bus):**
- Pins: GPIOC[9-12] (CS, SCK, MISO, MOSI)
- Clock speed: 36 MHz max (APB1 @ 72MHz / 2 prescaler)
- Mode: SPI Mode 0 (CPOL=0, CPHA=0)
- CS (PC9) controlled manually via GPIO
- Throughput: ~4.5 MB/s theoretical

**Important:** CH32V30x does NOT support Dual SPI or Quad SPI modes - only standard SPI (1-bit data transfer).

### ROM Loading Process
1. Initialize SD card (400kHz init, then 36MHz)
2. Mount FAT filesystem
3. Open `amiga.rom` file
4. Read 256KB into Flash buffer
5. Verify checksum (optional)
6. Close file and unmount

### I/O Port Registers
```
0x1F0000: SD_STATUS   (read: card status, busy, error flags)
0x1F0001: SD_COMMAND  (write: SD command to execute)
0x1F0002: SD_DATA     (read/write: data byte)
0x1F0003: SD_SECTOR_LO (write: sector number low 16 bits)
0x1F0004: SD_SECTOR_HI (write: sector number high 16 bits)
```

---

## 7. PCMCIA Attribute Memory (CIS)

### Standard PCMCIA Tuples
```c
const uint8_t attribute_memory[512] = {
    // CISTPL_DEVICE (Device Information)
    0x01, 0x03, 0xD9, 0x01, 0xFF,

    // CISTPL_VERS_1 (Version 1 Info)
    0x15, 0x2A, 0x04, 0x01,
    'A', 'M', 'I', 'G', 'A', 0x00,
    'P', 'C', 'M', 'C', 'I', 'A', ' ', 'C', 'a', 'r', 'd', 0x00,
    0xFF,

    // CISTPL_MANFID (Manufacturer ID)
    0x20, 0x04, 0x00, 0x00, 0x00, 0x00,

    // CISTPL_CONFIG (Configuration)
    0x1A, 0x05, 0x01, 0x03, 0x00, 0x02, 0x0F,

    // CISTPL_END
    0xFF
};
```

---

## 8. Timing Optimization

### Critical Path Optimizations
1. Use `#pragma GCC optimize("Ofast")` for handler
2. Inline all decode functions (`__attribute__((always_inline))`)
3. Use `restrict` keyword for buffer pointers
4. Direct register access (no HAL functions in ISR)
5. Read address/data early in handler to maximize decode time

### Meeting PCMCIA Spec (120ns)
- CH32V30x @ 144MHz = 6.94ns per cycle
- Budget: 120ns / 6.94ns = ~17 cycles maximum
- Current estimate: ~16 cycles (110ns)
- **Margin: ~10ns** (adequate for reliable operation)

---

## 9. SPI RAM Extension

### SPI RAM Implementation
- Use **SPI1** on GPIOA[1,5-7] for maximum speed (72 MHz vs 36 MHz on SPI3)
- PA1: SPIRAM_CS (chip select)
- PA5-PA7: SPI1 signals (SCK, MISO, MOSI)
- Implement banking for >2MB access if needed
- Use WAIT signal for wait states during SPI transfers
- Typical SPI RAM access @ 72 MHz: ~220ns (requires 1-2 PCMCIA wait cycles)

### WAIT Signal Management for SPI RAM
```c
uint16_t spi_ram_read(uint32_t addr) {
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);  // WAIT low (insert wait states)

    uint16_t data = spi_ram_transfer(addr);

    GPIO_SetBits(GPIOB, GPIO_Pin_14);  // WAIT high (no wait states)    // READY = high
    return data;
}
```

---

## 10. Build System Changes

### Linker Script (if needed)
- Ensure cartridge section at 0x08008000 is 256KB
- Verify SRAM allocation for stack/heap
- No changes needed if using existing MSX layout

### Makefile
- Update target name to reflect PCMCIA (optional)
- Add any new source files (pcmcia.c, sd_interface.c)
- No other changes required

---

## 11. Testing & Verification

### Unit Testing (Pre-Amiga)
1. **GPIO Configuration**: Verify pin modes with oscilloscope
2. **Address Bus**: Test all 22 address lines toggle correctly
3. **Data Bus**: Verify 16-bit read/write operations
4. **Interrupt Timing**: Measure CE1 to data output latency (<120ns)
5. **SD Card**: Verify ROM file loads correctly into Flash
6. **Attribute Memory**: Test CIS read operations

### Integration Testing (With Amiga)
1. **Card Detection**: Verify Amiga detects card via CD1/CD2
2. **Attribute Memory**: Amiga reads CIS successfully
3. **ROM Access**: Verify ROM reads at $600000 range
4. **I/O Ports**: Test SD card control via I/O registers
5. **Timing**: Confirm no wait states needed for ROM/attribute access
6. **Stability**: Extended operation test (hours)

### Debug Outputs
- PA10 status LED for operational state
- Serial debug via USART1 (PA10) if needed
- Programming mode entry via PA8

---

## 12. Key Implementation Notes

### Address Alignment
- 16-bit data bus requires even address alignment
- Amiga may issue byte or word accesses
- Handle both 8-bit and 16-bit access patterns

### Data Bus Tristate
- Always return to input mode after read cycle
- Prevent bus contention with Amiga
- No pull-ups needed (Amiga bus provides them)

### WAIT Signal (PCMCIA Pin 59, PB14)
- **Active LOW** - pulling low inserts wait states during bus cycle
- Keep HIGH for fast memory (ROM, attribute memory)
- Pull LOW for slow devices (SPI RAM access)
- Return HIGH when operation completes
- Gayle monitors WAIT and extends bus cycle as needed

### READY Signal (PCMCIA Pin 16, PA2)
- **Active HIGH** - indicates card is ready for new commands
- Keep HIGH during normal operation (card is ready)
- Pull LOW during long operations (Flash programming, SD card writes)
- Different from WAIT: READY is checked between operations, WAIT extends current cycle
- For a simple memory card, typically stays HIGH always

### Card Detect Signals (CD1/CD2)
- **PCMCIA Pins 36 & 63** - indicate card presence to Amiga
- **Active LOW** when card is inserted
- **Implementation:** Hard-wired to GND on PCB (permanent installation)
- **CH32 pins PB11/PB12** are used for IOR/IOW, NOT for CD1/CD2
- This tells the Amiga the card is always present (no hot-swap support)

### SD Card Detect (PA15)
- **PA15** monitors SD card insertion via mechanical switch on SD socket
- **Active LOW** when SD card is inserted (pulled to GND by socket switch)
- **Pull-up enabled** - reads HIGH when no card present
- Check this pin during initialization to verify SD card is present before attempting access

### Flash Programming Mode
- Detect via PA8 pin on startup
- Enter bootloader for ROM updates
- Allow ROM reprogramming from SD card

---

## 13. Migration from MSX Code

### Files to Heavily Modify
- gpio.c: Complete GPIO reconfiguration
- cart.c: Replace all handler logic
- main.c: Change initialization sequence

### Files to Remove/Disable
- scc.c/scc.h: SCC audio chip emulation (not needed)
- utils.c: Circular buffer (only if used for SCC)

### Files to Preserve
- Core peripheral drivers (unchanged)
- System initialization (minimal changes)
- Bootloader/IAP code (if present)

---

## Summary

This plan adapts the proven MSX cartridge interrupt-driven architecture to Amiga PCMCIA by:

1. **Expanding buses**: 16→22 address bits, 8→16 data bits
2. **Repurposing pins**: GPIOA LEDs/DAC → address lines A16-A21
3. **Adapting control signals**: MSX (SLT/RD/WR) → PCMCIA (CE/OE/WE/REG)
4. **Adding PCMCIA features**: Attribute memory CIS, I/O ports, SD integration
5. **Maintaining timing**: <110ns response meets 120ns PCMCIA spec
6. **Using Flash for ROM**: Preserves SRAM, enables fast access

**Architecture Decision**: GPIO interrupt-driven approach (NOT FSMC) due to:
- FSMC limited to 16-bit addressing (insufficient for 22-bit requirement)
- FSMC driver not implemented (would require complete development)
- GPIO approach proven in MSX implementation
- Meets timing requirements with margin
- Full control over PCMCIA-specific signaling
