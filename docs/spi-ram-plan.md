# APS6404L-3SQR SPI RAM Implementation Plan

## Device Overview

**Part Number:** APS6404L-3SQR-SN (SOP-8 package)
- **Capacity:** 64Mbit (8MB)
- **Interface:** SPI/QPI (using standard SPI mode only)
- **Supply Voltage:** 2.7V to 3.6V (using 3.3V)
- **Organization:** 8M x 8 bits, byte-addressable

## Performance Specifications

### Clock Frequencies (at VDD=3.3V)
- **Linear Burst Mode:** 84MHz maximum ✓ (our target mode)
- **Wrap-32 Mode:** 109MHz maximum (not needed)
- **SPI Read (0x03):** 33MHz maximum (slow mode)

### Operating Modes
- Device powers up in **SPI mode** by default (perfect for CH32V307)
- QPI mode available but not needed for our application
- Default burst mode: **Linear Burst** (crosses page boundaries up to 84MHz)

## Hardware Design Review

### Pin Configuration (SOP-8 Package)
| Pin | Signal | Function | Connection |
|-----|--------|----------|------------|
| 1 | /CE | Chip Select (active low) | CH32 GPIO (RAM_SNSS) |
| 2 | SO/SIO[1] | Serial Output / MISO | CH32 SPI MISO (RAM_MISO) |
| 3 | SIO[2] | Write Protect (unused in SPI) | Tie to VDD via 10k |
| 4 | VSS | Ground | GND |
| 5 | SI/SIO[0] | Serial Input / MOSI | CH32 SPI MOSI (RAM_MOSI) |
| 6 | SCLK | Serial Clock | CH32 SPI CLK (RAM_CLK) |
| 7 | SIO[3] | Hold (unused in SPI) | Tie to VDD via 10k |
| 8 | VDD | Power Supply | 3.3V |

### Critical Hardware Requirements

#### 1. Decoupling Capacitors (Section 14.3)
**REQUIRED:**
- **1µF ceramic capacitor** (low ESR) on VDD pin
- Place as close as possible to pin 8 (VDD)
- **Optional:** 0.1µF capacitor for better high-frequency response

**Schematic Status:** ✓ Verify placement on PCB layout

#### 2. Unused Pin Handling
**SIO[2] (Pin 3) - Write Protect:**
- Must be tied HIGH (VDD) to disable write protection in SPI mode
- Add 10kΩ pull-up resistor to VCC33

**SIO[3] (Pin 7) - Hold:**
- Must be tied HIGH (VDD) to disable hold function in SPI mode
- Add 10kΩ pull-up resistor to VCC33

**Schematic Status:** ✓ Pull-ups present on pins 3 and 7 (confirmed)

#### 3. PCB Layout Guidelines
- Keep decoupling caps within 5mm of VDD pin
- Minimize trace lengths on SPI signals
- Match trace lengths if possible (not critical at 84MHz)
- Solid ground plane under device
- Maximum load capacitance: 15pF per signal

## Firmware Implementation

### 1. Power-Up Initialization Sequence (Section 7)

```c
// After VDD reaches stable 3.3V:
// 1. Wait 150µs minimum
delay_us(150);

// 2. Ensure CLK is LOW, CE# is HIGH before starting
// 3. Issue Software Reset sequence

// Send Reset Enable command (0x66)
spi_ram_cs_low();
spi_transfer(0x66);
spi_ram_cs_high();

// Send Reset command (0x99)
spi_ram_cs_low();
spi_transfer(0x99);
spi_ram_cs_high();

// 4. Wait tRST (50ns minimum)
// 5. Device is now in SPI standby mode and ready
```

### 2. Command Termination (Section 8.6)

**CRITICAL REQUIREMENT:**
- All read/write operations MUST be terminated by raising CE# high
- Failure to do so blocks internal refresh operations → **memory corruption**
- CE# must be raised immediately after the last data byte

**Implementation:**
```c
// For writes:
spi_ram_cs_low();
spi_transfer(cmd);
spi_transfer(addr[0]);
spi_transfer(addr[1]);
spi_transfer(addr[2]);
spi_transfer(data);
spi_ram_cs_high();  // MUST raise CE# here!

// For reads:
spi_ram_cs_low();
spi_transfer(cmd);
spi_transfer(addr[0]);
spi_transfer(addr[1]);
spi_transfer(addr[2]);
// ... wait cycles if needed
data = spi_transfer(0xFF);
spi_ram_cs_high();  // MUST raise CE# here!
```

### 3. Timing Constraints (Section 14.6)

| Parameter | Min | Max | Notes |
|-----------|-----|-----|-------|
| Clock Period (tCLK) | 11.9ns | - | = 84MHz |
| CE# Setup (tCSP) | 2.5ns | - | Before CLK rising |
| CE# Hold (tCHD) | 3.0ns | - | After CLK rising |
| Data Setup (tSP) | 2ns | - | Before active CLK edge |
| Data Hold (tHD) | 2ns | - | After active CLK edge |
| CE# High Between Ops (tCPH) | 18ns | - | Between bursts |
| CE# Low Max (tCEM) | - | 8µs | Standard temp (-40 to 85°C) |
| CE# Low Max (tCEM) | - | 4µs | Extended temp (-40 to 105°C) |

**Key Implications:**
- Must raise CE# high for minimum 18ns between operations
- Cannot keep CE# low for more than 8µs (blocks refresh)
- For long transfers, break into smaller chunks with CE# toggling

### 4. SPI Commands

#### Fast Read (0x0B) - Recommended
- Clock up to 84MHz (linear burst)
- Requires 8 dummy cycles (wait cycles)
- Command format: `[0x0B][A23:A16][A15:A8][A7:A0][8 dummy cycles][Data...]`

```c
void spi_ram_read(uint32_t address, uint8_t *buffer, uint16_t length) {
    spi_ram_cs_low();

    spi_transfer(0x0B);  // Fast Read command
    spi_transfer((address >> 16) & 0xFF);  // A23:A16
    spi_transfer((address >> 8) & 0xFF);   // A15:A8
    spi_transfer(address & 0xFF);          // A7:A0

    // 8 dummy clock cycles
    spi_transfer(0xFF);

    // Read data
    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = spi_transfer(0xFF);
    }

    spi_ram_cs_high();
}
```

#### Write (0x02)
- Clock up to 84MHz (linear burst)
- No wait cycles required
- Command format: `[0x02][A23:A16][A15:A8][A7:A0][Data...]`

```c
void spi_ram_write(uint32_t address, const uint8_t *buffer, uint16_t length) {
    spi_ram_cs_low();

    spi_transfer(0x02);  // Write command
    spi_transfer((address >> 16) & 0xFF);  // A23:A16
    spi_transfer((address >> 8) & 0xFF);   // A15:A8
    spi_transfer(address & 0xFF);          // A7:A0

    // Write data
    for (uint16_t i = 0; i < length; i++) {
        spi_transfer(buffer[i]);
    }

    spi_ram_cs_high();
}
```

#### Slow Read (0x03) - For Debugging
- Clock up to 33MHz only
- No wait cycles
- Useful for initial testing at lower speeds

### 5. Address Space (Section 8.1)
- Byte-addressable: A[22:0] (23 address bits)
- Page size: 1024 bytes (1KB)
- Linear burst can cross page boundaries at 84MHz max
- Can cross page boundary one time only per burst

### 6. Best Practices

**DO:**
- Always terminate operations with CE# high
- Wait minimum 18ns between operations
- Break long transfers into chunks (< 8µs CE# low time)
- Initialize with proper reset sequence on power-up
- Use Fast Read (0x0B) for best performance

**DON'T:**
- Don't keep CE# low for more than 8µs continuous
- Don't forget dummy cycles on Fast Read (0x0B)
- Don't exceed 84MHz in linear burst mode at 3.3V
- Don't skip the power-up initialization sequence

## Current Consumption

| Mode | Typical | Maximum | Notes |
|------|---------|---------|-------|
| Active (Read/Write) | 5.5mA @ 133MHz | 7mA | At 84MHz will be lower |
| Standby | 100µA @ 25°C | 250µA @ 85°C | CLK in DC low state |

## Testing Plan

### Phase 1: Basic Connectivity
1. Verify SPI communication at low speed (1MHz)
2. Read Device ID (command 0x9F) if available
3. Simple write/read test at single address

### Phase 2: Speed Testing
1. Test at 10MHz
2. Test at 42MHz (half speed)
3. Test at 84MHz (full speed)
4. Verify data integrity at each speed

### Phase 3: Functional Testing
1. Sequential read/write operations
2. Random access patterns
3. Page boundary crossing
4. Long burst operations (with CE# management)
5. Temperature testing if needed

## Schematic Review Checklist

- [ ] 1µF decoupling capacitor on VDD pin (as close as possible)
- [ ] Optional 0.1µF decoupling capacitor
- [x] Pull-up resistor on SIO[2] (pin 3) to VCC33 ✓
- [x] Pull-up resistor on SIO[3] (pin 7) to VCC33 ✓
- [x] SPI signals routed to CH32V307 ✓
- [x] RAM_SNSS (CS) connected to GPIO ✓
- [x] RAM_CLK connected to SPI CLK ✓
- [x] RAM_MOSI connected to SPI MOSI ✓
- [x] RAM_MISO connected to SPI MISO ✓
- [x] Ground and power properly connected ✓

## References

- APS6404L-3SQR Datasheet Rev. 2.3 (April 30, 2020)
- Located: `/Users/stephen/git/tfpcmcia/docs/APS6404L_3SQR.pdf`
- Key sections: 7 (Power-Up), 8 (Interface), 10 (SPI Operations), 14 (Electrical)

## Notes from Schematic Review

- Note in schematic states: "Unfortunately the CH32 chips don't do QUAD SPI"
  - ✓ Confirmed: Device works perfectly in standard SPI mode (default mode)
  - ✓ No QSPI required for full functionality

- Note states: "Using the footprint from a standard SPI ROM but this will work for the APS6404L"
  - ✓ Verified: Pin assignments match standard SPI ROM footprints
  - ⚠️ Must verify pull-ups on unused pins (SIO[2], SIO[3])

## Status

**Hardware:** Design verified compatible with CH32V307
**Firmware:** Implementation plan complete, ready for coding
**Testing:** Awaiting PCB review and firmware implementation

---
Document created: 2026-01-29
Last updated: 2026-01-29
