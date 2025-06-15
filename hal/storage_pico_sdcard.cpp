#include "storage_pico_sdcard.h"

#include <hardware/gpio.h>
#include <hardware/spi.h>

#include <stdio.h>

// based on https://github.com/oyama/pico-vfs/blob/main/src/blockdevice/sd.c
// which was in turn based on https://github.com/ARMmbed/mbed-os/blob/master/storage/blockdevice/COMPONENT_SD/source/SDBlockDevice.cpp

#define SPI_CMD(x) (0x40 | ((x) & 0x3f))

/* R1 Response Format */
#define R1_NO_RESPONSE          (0xFF)
#define R1_RESPONSE_RECV        (0x80)
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)

enum cmd_supported {
    CMD_NOT_SUPPORTED = -1,             /**< Command not supported error */
    CMD0_GO_IDLE_STATE = 0,             /**< Resets the SD Memory Card */
    CMD1_SEND_OP_COND = 1,              /**< Sends host capacity support */
    CMD6_SWITCH_FUNC = 6,               /**< Check and Switches card function */
    CMD8_SEND_IF_COND = 8,              /**< Supply voltage info */
    CMD9_SEND_CSD = 9,                  /**< Provides Card Specific data */
    CMD10_SEND_CID = 10,                /**< Provides Card Identification */
    CMD12_STOP_TRANSMISSION = 12,       /**< Forces the card to stop transmission */
    CMD13_SEND_STATUS = 13,             /**< Card responds with status */
    CMD16_SET_BLOCKLEN = 16,            /**< Length for SC card is set */
    CMD17_READ_SINGLE_BLOCK = 17,       /**< Read single block of data */
    CMD18_READ_MULTIPLE_BLOCK = 18,     /**< Card transfers data blocks to host until interrupted
                                             by a STOP_TRANSMISSION command */
    CMD24_WRITE_BLOCK = 24,             /**< Write single block of data */
    CMD25_WRITE_MULTIPLE_BLOCK = 25,    /**< Continuously writes blocks of data until
                                             'Stop Tran' token is sent */
    CMD27_PROGRAM_CSD = 27,             /**< Programming bits of CSD */
    CMD32_ERASE_WR_BLK_START_ADDR = 32, /**< Sets the address of the first write
                                             block to be erased. */
    CMD33_ERASE_WR_BLK_END_ADDR = 33,   /**< Sets the address of the last write
                                             block of the continuous range to be erased.*/
    CMD38_ERASE = 38,                   /**< Erases all previously selected write blocks */
    CMD55_APP_CMD = 55,                 /**< Extend to Applications specific commands */
    CMD56_GEN_CMD = 56,                 /**< General Purpose Command */
    CMD58_READ_OCR = 58,                /**< Read OCR register of card */
    CMD59_CRC_ON_OFF = 59,              /**< Turns the CRC option on or off*/
    // App Commands
    ACMD6_SET_BUS_WIDTH = 6,
    ACMD13_SD_STATUS = 13,
    ACMD22_SEND_NUM_WR_BLOCKS = 22,
    ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
    ACMD41_SD_SEND_OP_COND = 41,
    ACMD42_SET_CLR_CARD_DETECT = 42,
    ACMD51_SEND_SCR = 51,
};

#define PACKET_SIZE   6  // SD Packet size CMD+ARG+CRC


namespace hal {

static const uint8_t SPI_FILL_CHAR = 255;

inline void storage_pico_sdcard::_preclock_then_select() {
    spi_write_blocking(m_spi_inst, &SPI_FILL_CHAR, 1);
    gpio_put(m_cs_pin, 0);
}

inline void storage_pico_sdcard::_postclock_then_deselect() {
    spi_write_blocking(m_spi_inst, &SPI_FILL_CHAR, 1);
    gpio_put(m_cs_pin, 1);
}

inline void storage_pico_sdcard::_cmd(int cmd, uint32_t arg, bool is_acmd, uint32_t *outOptResp) {

}

storage_pico_sdcard* storage_pico_sdcard::create(uint8_t spi,
		uint8_t sd_clk_pin,uint8_t sd_mosi_pin,uint8_t sd_miso_pin,
		uint8_t sd_cs_pin,uint8_t sd_det_pin) {
    
    storage_pico_sdcard *result = new storage_pico_sdcard;
    struct spi_inst *spi_inst = spi? spi1 : spi0;
    result->m_spi_inst = spi_inst;
    result->m_clk_pin = sd_clk_pin;
    result->m_mosi_pin = sd_mosi_pin;
    result->m_miso_pin = sd_miso_pin;
    result->m_cs_pin = sd_cs_pin;
    result->m_det_pin = sd_det_pin;

    gpio_set_function(sd_mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(sd_miso_pin, GPIO_FUNC_SPI);
    gpio_set_function(sd_clk_pin, GPIO_FUNC_SPI);
    gpio_init(sd_cs_pin);
    gpio_set_dir(sd_cs_pin, GPIO_OUT);
    gpio_pull_up(sd_miso_pin);
    gpio_set_drive_strength(sd_mosi_pin, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_drive_strength(sd_clk_pin, GPIO_DRIVE_STRENGTH_4MA);
    spi_init(spi_inst, 10'000'000);
    spi_set_format(spi_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(sd_cs_pin, 1);
    bool old_cs = gpio_get(sd_cs_pin);
    for (int i = 0; i < 10; i++)
        // could be spi_write_blocking?
        spi_write_read_blocking(spi_inst, &SPI_FILL_CHAR, nullptr, 1);
    gpio_put(sd_cs_pin, old_cs);

    uint32_t response = ~R1_IDLE_STATE;
    for (int i=0; i<5 && response != R1_IDLE_STATE; i++)
        result->_cmd(CMD0_GO_IDLE_STATE, 0x0, false, &response);
    if (response != R1_IDLE_STATE) {
        printf("sdcard - failed to return to idle state\n");
        delete result;
        return nullptr;
    }

    printf("sdcard - created\n");
    return result;
}

void storage_pico_sdcard::init() {
}

void storage_pico_sdcard::flush(){
}

size_t storage_pico_sdcard::getBlockSize() const {
    return 512;
}

size_t storage_pico_sdcard::getBlockCount() const {
    return 0;
}

bool storage_pico_sdcard::readBlock(size_t index,void *dest) {
    return false;
}

bool storage_pico_sdcard::writeBlock(size_t index,const void *dest) {
    return false;
}

const void *storage_pico_sdcard::memoryMap(size_t index,size_t blockCount) {
    return nullptr;    
}

}
