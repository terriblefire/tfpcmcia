#include "pcmcia.h"
#include "gpio.h"
#include "spiram.h"

#define NDEBUG

#ifdef NDEBUG
#define RAMFUNC __attribute__((section(".ramtext")))
#else
#define RAMFUNC
#endif         

#define GPIO_SafeSetBits(GPIOx, GPIO_Pin) ((GPIOx)->BSHR = (GPIO_Pin))
#define GPIO_SafeResetBits(GPIOx, GPIO_Pin) ((GPIOx)->BCR = (GPIO_Pin))
#define GPIO_SafeGetBits(GPIOx, GPIO_Pin) ((GPIOx)->INDR & (GPIO_Pin))

#define SD_CS_HIGH() {GPIO_SafeSetBits(SD_SNSS_GPIO_Port, SD_SNSS_Pin); GPIO_SafeSetBits(LED2_GPIO_Port, LED2_Pin);}
#define SD_CS_LOW()  {GPIO_SafeResetBits(SD_SNSS_GPIO_Port, SD_SNSS_Pin); GPIO_SafeResetBits(LED2_GPIO_Port, LED2_Pin);}

#define DELAY_100NS() do { \
    __asm volatile ("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"); \
} while (0)

typedef struct {
    /* GPIO port D configuration words */
    uint32_t BusOn;      /* 0x33333333 — CFGLR/CFGHR: output 50 MHz push-pull */
    uint32_t BusOff;     /* 0x44444444 — CFGLR/CFGHR: input floating          */
    /* ROM address space */
    uint32_t RomSize;    /* 0x020000   — 128 KB ROM upper bound (exclusive)    */
    uint32_t RomMask;    /* 0x1FFFE    — word-align mask within 128 KB         */
    /* I/O address space */
    uint32_t IoBase;     /* 0x220000   — first address decoded as I/O          */
    uint32_t IoRegBase;  /* 0x220200   — first address with I/O registers      */
    /* GPIOB control-signal masks */
    uint32_t AccessMask; /* 0x001980   — OE | WE | IOR | IOW                  */
    uint32_t IorMask;    /* 0x000800   — IOR bit                               */
    uint32_t IowMask;    /* 0x001000   — IOW bit                               */
    /* SPIRAM address space */
    uint32_t SpiRamBase; /* 0x020000   — SPIRAM region start                  */
    uint32_t SpiRamMask; /* 0x3FFFFE   — word-align mask within 4 MB SPIRAM   */
    uint32_t SpiRamSize; /* 0x400000   — SPIRAM upper bound (exclusive)        */
} PcmciaConstants;

static PcmciaConstants K __attribute__((section(".sdata")));

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



/* CIS tuples for Amiga autoboot (CISTPL_AMIGAXIP with AUTORUN flag).
 * Attribute memory is byte-wide; each byte lives at an even address.
 * Index into cis_data[] using byte_offset >> 1 where byte_offset = addr & 0xFFFF. */
static uint8_t cis_data[] = {
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
#define REG_LED_FB_BASE 0x220u   /* 0xA20220 → byte_offset 0x0220 */

#define BOARD_ID_VALUE  0x01u

static uint8_t pcmica_board_ctrl = 0;

uint32_t led_fb[7];               /* RGBX pixels; host writes via IO writes */

static __attribute__((always_inline)) uint16_t PCMCIA_ROM_Read(uint32_t addr) {
    uint32_t offset = addr & K.RomMask;  /* word-align within 128KB ROM */
    return ((uint16_t)boot_rom[offset] << 8) | boot_rom[offset + 1];
}
static __attribute__((always_inline)) uint16_t PCMCIA_Reg_Read(uint32_t addr) {
    
    uint32_t offset = addr & K.RomMask;  /* word-align within 128KB ROM */

    if (offset < 0x200u) {
        uint8_t b;
        uint32_t idx = offset >> 1;
        b = (idx < CIS_LEN) ? cis_data[idx] : 0xFFu;
        return (uint16_t)b << 8;  /* big-endian: byte on upper (even) lane */
    }

    return 0;
}

static __attribute__((always_inline)) uint16_t PCMCIA_IO_Read(uint32_t addr) {
    
    uint32_t byte_offset = addr & 0xFFFFu;

    switch (byte_offset) {
        case REG_SPI_DATA:   return (uint16_t) sd_rx_byte << 8;
        case REG_SPI_CS:     return (GPIO_SafeGetBits(SD_SNSS_GPIO_Port, SD_SNSS_Pin) ? 1 : 0) << 8;
        case REG_SPI_STATUS: {
            return (uint16_t) SD_CARD_PRESENT() << 8;
        }
        case REG_BOARD_CTRL: return (uint16_t)pcmica_board_ctrl << 8;
        case REG_BOARD_ID:   return (uint16_t)BOARD_ID_VALUE << 8;
        default:             return 0xFF00u;
    }
}

static __attribute__((always_inline)) void PCMCIA_IO_Write(uint32_t addr, uint16_t data) {
    uint32_t byte_offset = addr & 0xFFFFu;
    uint8_t b = (uint8_t)(data >> 8);  /* driver byte is on upper lane */

    if (byte_offset >= REG_LED_FB_BASE && byte_offset < REG_LED_FB_BASE + 28u) {
        /* Each pixel = 4 bytes, accessed as two consecutive 16-bit writes.
         * word_idx 0,1 = pixel 0 (R,G then B,X); 2,3 = pixel 1; etc. */
        uint8_t word_idx = (uint8_t)((byte_offset - REG_LED_FB_BASE) >> 1); /* 0..13 */
        uint8_t pix_idx  = word_idx >> 1;                                    /* 0..6  */
        if ((word_idx & 1u) == 0u) {
            /* high word: data[15:8]=R, data[7:0]=G */
            led_fb[pix_idx] = (led_fb[pix_idx] & 0x0000FFFFu) | ((uint32_t)data << 16);
        } else {
            /* low word: data[15:8]=B, data[7:0]=X */
            led_fb[pix_idx] = (led_fb[pix_idx] & 0xFFFF0000u) | (uint32_t)data;
        }
        return;
    }

    switch (byte_offset) {
        case REG_BOARD_CTRL: 
            pcmica_board_ctrl = b;
            break; 
        case REG_SPI_DATA:
            sd_xfer(b);
            break;
        case REG_SPI_CS:
            pcmica_board_ctrl = 0;
            b &= 0x01;
            if (b == 0x00u) SD_CS_LOW() else SD_CS_HIGH();
            break;
        default:
            break;
    }
}

/* ---- Interrupt handler -------------------------------------------------- */

void PCMCIA_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast"), flatten, section(".ramtext")));

void PCMCIA_Handler(void) {
    /* Assert /WAIT immediately — stalls the Amiga until we deassert. */
    GPIO_SafeResetBits(GPIOB, GPIO_Pin_14);
    EXTI->INTFR = EXTI_Line6 | EXTI_Line9;

    uint16_t ctrl = (uint16_t)GPIOB->INDR;
    // recheck the strobes incase of glitches
    if ((ctrl & STROBE_MASK) == STROBE_MASK) goto exit;
    ctrl = (uint16_t)GPIOB->INDR;

    // get the address from the ports 
    uint32_t addr  = ((uint32_t)(ctrl & 0x003Fu) << 16) | (uint16_t)GPIOE->INDR;
    uint16_t memory_value = ((ctrl & REG_MASK) == 0) ? PCMCIA_Reg_Read(addr) : PCMCIA_ROM_Read(addr);
    GPIOD->OUTDR = BUS16(memory_value);

    if (((ctrl & REG_MASK) == 0) && (addr < K.IoBase))
    {   
        GPIOD->CFGLR = K.BusOn;
        GPIOD->CFGHR = K.BusOn;
        goto exit;
    }

    do 
    {
        ctrl = (uint16_t)GPIOB->INDR;
    } while ((ctrl & K.AccessMask) == K.AccessMask);
    
    if ((ctrl & OE_MASK) == 0) 
    {
        // output enable set.
        GPIOD->CFGLR = K.BusOn;
        GPIOD->CFGHR = K.BusOn;

        if ((ctrl & REG_MASK) != 0)
        {
            if (addr < K.RomSize)
            {
                GPIOD->OUTDR = BUS16(PCMCIA_ROM_Read(addr));
            }
            else 
            {
                GPIOD->OUTDR = BUS16(SPIRAM_Read16(addr & K.SpiRamMask));
            }
        }

        DELAY_100NS();
    } 
    else if ((ctrl & WE_MASK) == 0)
    {   
        GPIOD->CFGLR = K.BusOff;
        GPIOD->CFGHR = K.BusOff;
        DELAY_100NS();
        
        uint16_t data = BUS16((uint16_t)GPIOD->INDR);
        
        if ((ctrl & STROBE_MASK) == 0)
        {
            SPIRAM_Write16(addr, data);
        }
        else if ((ctrl & UDS_MASK) == 0)
        {
            SPIRAM_Write8(addr, (data >> 8));
        }
        else if ((ctrl & LDS_MASK) == 0)
        {
            SPIRAM_Write8(addr | 0x01, data & 0xFF);
        }
    }
    else if ((ctrl & K.IowMask) == 0)
    {
        GPIOD->CFGLR = K.BusOff;
        GPIOD->CFGHR = K.BusOff;
        DELAY_100NS();

        if (addr >= K.IoRegBase)
        {
            // Write cycle: GPIOD stays floating; read data the Amiga is driving
            uint16_t data = BUS16((uint16_t)GPIOD->INDR);
            PCMCIA_IO_Write(addr, data);
        }
    }
    else if ((ctrl & K.IorMask) == 0)
    {
        // output enable set.
        GPIOD->CFGLR = K.BusOn;
        GPIOD->CFGHR = K.BusOn;

        if (addr >= K.IoRegBase)
        {
            // output enable set.
            uint16_t val = PCMCIA_IO_Read(addr);
            GPIOD->OUTDR = BUS16(val);
            GPIOD->CFGLR = K.BusOn;
            GPIOD->CFGHR = K.BusOn;    
        }

        DELAY_100NS();
    }
exit:
    GPIO_SafeSetBits(GPIOB, GPIO_Pin_14);
}

uint8_t PCMCIA_BoardCtrl(void) { return pcmica_board_ctrl; }

/* ---- Init --------------------------------------------------------------- */

void PCMCIA_Init(void) {

    K.BusOn      = 0x33333333u;
    K.BusOff     = 0x44444444u;
    K.RomSize    = 0x020000u;
    K.RomMask    = 0x1FFFEu;
    K.IoBase     = 0x220000u;
    K.IoRegBase  = 0x220200u;
    K.AccessMask = ACCESS_MASK;
    K.IorMask    = IOR_MASK;
    K.IowMask    = IOW_MASK;
    K.SpiRamBase = 0x020000u;
    K.SpiRamMask = 0x3FFFFEu;
    K.SpiRamSize = 0x400000u;

    GPIO_SetBits(GPIOB, GPIO_Pin_14); // !WAIT
    GPIO_SetBits(GPIOA, GPIO_Pin_2); // READY
  
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SetVTFIRQ((u32)PCMCIA_Handler, EXTI9_5_IRQn, 0, ENABLE);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void RAMFUNC PCMCIA_PollLoop(void)
{
    uint16_t ctrl;

    while (1) 
    {
        ctrl = (uint16_t)GPIOB->INDR;
        
        if ((ctrl & STROBE_MASK) != STROBE_MASK)
        {
            PCMCIA_Handler();
        }
    }
}