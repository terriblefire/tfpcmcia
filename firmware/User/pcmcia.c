#include "pcmcia.h"
#include "gpio.h"

#define GPIO_SafeSetBits(GPIOx, GPIO_Pin) ((GPIOx)->BSHR = (GPIO_Pin))
#define GPIO_SafeResetBits(GPIOx, GPIO_Pin) ((GPIOx)->BCR = (GPIO_Pin))
#define GPIO_SafeGetBits(GPIOx, GPIO_Pin) ((GPIOx)->INDR & (GPIO_Pin))

#define SD_CS_HIGH() {GPIO_SafeSetBits (SD_SNSS_GPIO_Port, SD_SNSS_Pin);} 
#define SD_CS_LOW() {GPIO_SafeResetBits (SD_SNSS_GPIO_Port, SD_SNSS_Pin);}

#define DELAY_100NS() do { \
    __asm volatile ("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop"); \
} while (0)

/* ---- Dispatch functions -------------------------------------------------
 * Must be always_inline: PCMCIA_Handler uses WCH-Interrupt-fast which does
 * not save/restore caller-saved registers. Calling a normal function from
 * such a handler corrupts a0-a7, t0-t6, ra — producing garbage results.
 * always_inline eliminates the call entirely. */

extern const uint8_t boot_rom[];

static __attribute__((always_inline)) FlagStatus MYSPI_I2S_GetFlagStatus(SPI_TypeDef *SPIx, uint16_t SPI_I2S_FLAG)
{
    FlagStatus bitstatus = RESET;

    if((SPIx->STATR & SPI_I2S_FLAG) != (uint16_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}

#define SD_CARD_PRESENT() ((SD_CD_GPIO_Port->INDR & SD_CD_Pin) == 0 ? 1u : 0u)  /* LOW = card inserted */

static uint8_t sd_rx_byte = 0xFF;

static __attribute__((always_inline)) uint8_t sd_xfer(uint8_t data) {
    
    while (SPI_I2S_GetFlagStatus (SPI3, SPI_I2S_FLAG_TXE) == RESET) {};
    SPI3->DATAR = data;
    while (SPI_I2S_GetFlagStatus (SPI3, SPI_I2S_FLAG_RXNE) == RESET) {};
    sd_rx_byte = SPI3->DATAR;
    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_BSY) == SET) {};  // wait for shift register idle
    return sd_rx_byte;
}



static __attribute__((always_inline)) uint16_t PCMCIA_Memory_Read(uint32_t addr) {
    if (addr >= 0x10000u) {
        return 0;
    }
    uint32_t offset = addr & 0xFFFEu;  /* word-align within 64KB ROM */
    return ((uint16_t)boot_rom[offset] << 8) | boot_rom[offset + 1];
}

/* CIS tuples for Amiga autoboot (CISTPL_AMIGAXIP with AUTORUN flag).
 * Attribute memory is byte-wide; each byte lives at an even address.
 * Index into cis_data[] using byte_offset >> 1 where byte_offset = addr & 0xFFFF. */
static const uint8_t cis_data[] = {
    /* CISTPL_DEVICE (0x01): SRAM, 250ns, 4MB */
    0x01, 0x03, 0xd1, 0x27, 0xff,

    /* CISTPL_VERS_1 (0x15): version 4.1, manufacturer, product, version strings */
    0x15, 0x22,
    0x04, 0x01,
    'T','e','r','r','i','b','l','e','F','i','r','e', 0x00,
    'P','C','M','C','I','A',' ','S','D','+','R','A','M', 0x00,
    '1','.','0', 0x00,
    0xff,

    /* CISTPL_FUNCID (0x21): memory card */
    0x21, 0x02, 0x01, 0x00,

    /* CISTPL_AMIGAXIP (0x91): execute-in-place, AUTORUN flag set */
    0x91, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,

    /* CISTPL_END */
    0xff,
};

#define CIS_LEN  ((uint32_t)(sizeof(cis_data)))

/* Register offsets (byte_offset = addr & 0xFFFF, MAME word-offset * 2) */
#define REG_SPI_DATA    0x200u
#define REG_SPI_CS      0x202u
#define REG_SPI_STATUS  0x204u
#define REG_BOARD_CTRL  0x206u
#define REG_BOARD_ID    0x208u

#define BOARD_ID_VALUE  0x01u

static __attribute__((always_inline)) uint16_t PCMCIA_Reg_Read(uint32_t addr) {
    uint32_t byte_offset = addr & 0xFFFFu;

    if (byte_offset < 0x200u) {
        /* CIS space: each CIS byte at an even address */
        uint32_t idx = byte_offset >> 1;
        uint8_t b = (idx < CIS_LEN) ? cis_data[idx] : 0xFFu;
        return (uint16_t)b << 8;  /* big-endian: byte on upper (even) lane */
    }
}

static __attribute__((always_inline)) uint16_t PCMCIA_IO_Read(uint32_t addr) {
    
    uint32_t byte_offset = addr & 0xFFFFu;

    switch (byte_offset) {
        case REG_SPI_DATA:   return (uint16_t) sd_rx_byte << 8;
        case REG_SPI_CS:     return (GPIO_SafeGetBits(SD_SNSS_GPIO_Port, SD_SNSS_Pin) ? 1 : 0) << 8;
        case REG_SPI_STATUS: {
            return (uint16_t) SD_CARD_PRESENT() << 8;
        }
        case REG_BOARD_CTRL: return 0x0000u;
        case REG_BOARD_ID:   return (uint16_t)BOARD_ID_VALUE << 8;
        default:             return 0xFF00u;
    }
}

static __attribute__((always_inline)) void PCMCIA_Memory_Write(uint32_t addr, uint16_t data) {
    if (addr >= 0x10000u) {
        //SPIRAM_Write16(addr - 0x10000u, data);
    }
    /* writes to ROM region ignored */
}

static __attribute__((always_inline)) void PCMCIA_IO_Write(uint32_t addr, uint16_t data) {
    uint32_t byte_offset = addr & 0xFFFFu;
    uint8_t b = (uint8_t)(data >> 8);  /* driver byte is on upper lane */

    switch (byte_offset) {
        case REG_SPI_DATA:
            sd_xfer(b);
            break;
        case REG_SPI_CS:
            b &= 0x01;
            if (b == 0x00u) SD_CS_LOW() else SD_CS_HIGH();
            break;
        default:
            break;
    }
}

/* ---- Interrupt handler -------------------------------------------------- */

//void PCMCIA_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast"), flatten));

void PCMCIA_Handler(void) {
    /* Assert /WAIT immediately — stalls the Amiga until we deassert.
     * Must be first: VTF latency is ~14 ns; Amiga samples at ~150 ns.
     * The flash ROM lookup would otherwise push us past the sample point. */
    GPIO_SafeResetBits(GPIOB, GPIO_Pin_14);

    EXTI->INTFR = EXTI_Line6 | EXTI_Line9;

    uint16_t ctrl = (uint16_t)GPIOB->INDR;
    uint32_t addr  = ((uint32_t)(ctrl & 0x003Fu) << 16) | (uint16_t)GPIOE->INDR;
    uint16_t memory_value = ((ctrl & REG_MASK) == 0) ? PCMCIA_Reg_Read(addr) : PCMCIA_Memory_Read(addr);
    GPIOD->OUTDR = BUS16(memory_value);

    do 
    {
        ctrl = (uint16_t)GPIOB->INDR;
    } while ((ctrl & ACCESS_MASK) == ACCESS_MASK);
    
    if ((ctrl & OE_MASK) == 0) 
    {
        // output enable set. 
        GPIOD->CFGLR = 0x33333333;
        GPIOD->CFGHR = 0x33333333;
        GPIO_SafeSetBits(GPIOB, GPIO_Pin_14);
        while ((GPIOB->INDR & OE_MASK) != OE_MASK) {}
        GPIOD->CFGLR = 0x44444444;
        GPIOD->CFGHR = 0x44444444;
    } 
    else if ((ctrl & IOW_MASK) == 0)
    {
        if (addr >= 0x220200)  
        {
            // Write cycle: GPIOD stays floating; read data the Amiga is driving 
            uint16_t data = BUS16((uint16_t)GPIOD->INDR);
            PCMCIA_IO_Write(addr, data);
            //printf("IO Write: %08X = 0x%04X\n", addr, data);
            GPIO_SafeSetBits(GPIOB, GPIO_Pin_14);
            while ((GPIOB->INDR & IOW_MASK) != IOW_MASK) {}    
        }            
    }
    else if ((ctrl & IOR_MASK) == 0)
    {   
        if (addr >= 0x220200)  
        {
            // output enable set. 
            uint16_t val = PCMCIA_IO_Read(addr);
            //printf("IO Read: %08X=0x%04X\n", addr, val);

            GPIOD->OUTDR = BUS16(val);
            GPIOD->CFGLR = 0x33333333;
            GPIOD->CFGHR = 0x33333333;
            DELAY_100NS();
            GPIO_SafeSetBits(GPIOB, GPIO_Pin_14);
            while ((GPIOB->INDR & IOR_MASK) != IOR_MASK) {}
            GPIOD->CFGLR = 0x44444444;
            GPIOD->CFGHR = 0x44444444;
        }
    }

    GPIO_SafeSetBits(GPIOB, GPIO_Pin_14);
}

/* ---- Init --------------------------------------------------------------- */

void Init_PCMCIA(void) {

    GPIO_SetBits(GPIOB, GPIO_Pin_14); // !WAIT
    GPIO_SetBits(GPIOA, GPIO_Pin_2); // READY
  
    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    //SetVTFIRQ((u32)PCMCIA_Handler, EXTI9_5_IRQn, 0, ENABLE);
    //NVIC_SetPriority(EXTI9_5_IRQn, 0);
    //NVIC_EnableIRQ(EXTI9_5_IRQn);
}
