#include "spiram.h"

#define APS_CMD_RSTEN   0x66u
#define APS_CMD_RST     0x99u

void SPIRAM_Init(void) {
    /* SPI1 + GPIO configured by GPIO_Config() / GPIO_Spi_Init() */

    /* Reset sequence: RST_EN then RST */
    SPIRAM_CS_LOW();
    spiram_xfer(APS_CMD_RSTEN);
    SPIRAM_CS_HIGH();

    SPIRAM_CS_LOW();
    spiram_xfer(APS_CMD_RST);
    SPIRAM_CS_HIGH();
}

void SPIRAM_Test(void) {

    SPIRAM_CS_LOW();
    spiram_xfer(0x9F);
    uint8_t id = spiram_xfer(0x00);
    uint8_t kgd = spiram_xfer(0x00);
    SPIRAM_CS_HIGH();
    
    printf("Read ID: %02x - %02x\n", id, kgd);
}
