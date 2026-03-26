#pragma once

#include <exec/types.h>

/* Forward declaration — callers pass db->db_SysBase; spi.c ignores it for now */
struct ExecBase;

// PCMCIA attribute memory registers (even byte addresses)
#define SPI_DATA   (*(volatile UBYTE*)0xA20200)
#define SPI_CS     (*(volatile UBYTE*)0xA20202)
#define SPI_STATUS (*(volatile UBYTE*)0xA20204)
#define BOARD_CTRL (*(volatile UBYTE*)0xA20206)

// SD card commands
#define CMD0   0x40  // GO_IDLE_STATE
#define CMD1   0x41  // SEND_OP_COND (MMC)
#define CMD8   0x48  // SEND_IF_COND
#define CMD13  0x4D  // SEND_STATUS
#define CMD16  0x50  // SET_BLOCKLEN
#define CMD17  0x51  // READ_SINGLE_BLOCK
#define CMD24  0x58  // WRITE_BLOCK
#define CMD55  0x77  // APP_CMD
#define CMD58  0x7A  // READ_OCR
#define ACMD41 0x69  // SD_SEND_OP_COND

// SPI/SD function prototypes
void  SdSetSysBase(struct ExecBase* sb);
UBYTE SpiByte(UBYTE data);
UBYTE SdInit(void);
LONG  SdReadSector(UBYTE cardType, ULONG lba, UBYTE* buffer);
LONG  SdWriteSector(UBYTE cardType, ULONG lba, const UBYTE* buffer);
