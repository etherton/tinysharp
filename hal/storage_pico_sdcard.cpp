#include "storage_pico_sdcard.h"

#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <pico/time.h>

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

enum cmds : uint8_t {
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

/* R3 Response : OCR Register */
#define OCR_HCS_CCS             (1 << 30)
#define OCR_LOW_VOLTAGE         (1 << 24)
#define OCR_3_3V                (1 << 20)

enum sdcard_type: uint8_t {
   SDCARD_NONE  = 0,
   SDCARD_V1    = 1,
   SDCARD_V2    = 2,
   SDCARD_V2HC  = 3,
   SDCARD_UNKNOWN = 4,
};

#define SD_BLOCK_DEVICE_ERROR_OK                  0
#define SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK        -5001  /*!< operation would block */
#define SD_BLOCK_DEVICE_ERROR_UNSUPPORTED        -5002  /*!< unsupported operation */
#define SD_BLOCK_DEVICE_ERROR_PARAMETER          -5003  /*!< invalid parameter */
#define SD_BLOCK_DEVICE_ERROR_NO_INIT            -5004  /*!< uninitialized */
#define SD_BLOCK_DEVICE_ERROR_NO_DEVICE          -5005  /*!< device is missing or not connected */
#define SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED    -5006  /*!< write protected */
#define SD_BLOCK_DEVICE_ERROR_UNUSABLE           -5007  /*!< unusable card */
#define SD_BLOCK_DEVICE_ERROR_NO_RESPONSE        -5008  /*!< No response from device */
#define SD_BLOCK_DEVICE_ERROR_CRC                -5009  /*!< CRC error */
#define SD_BLOCK_DEVICE_ERROR_ERASE              -5010  /*!< Erase error: reset/sequence */
#define SD_BLOCK_DEVICE_ERROR_WRITE              -5011  /*!< SPI Write error: !SPI_DATA_ACCEPTED */

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

inline uint8_t storage_pico_sdcard::_spi_write_read(uint8_t value) {
    uint8_t resp;
    spi_write_read_blocking(m_spi_inst,&value,&resp,1);
    return resp;
}

inline bool storage_pico_sdcard::_wait_token(uint8_t token,uint32_t timeoutMs) {
    uint32_t stop = to_ms_since_boot(get_absolute_time()) + timeoutMs;
    do {
        if (token == _spi_write_read())
            return true;
    } while (to_ms_since_boot(get_absolute_time()) < stop);
    return false;

}

uint8_t storage_pico_sdcard::_cmd_spi(uint8_t cmd, uint32_t arg) {
    uint8_t response;
    uint8_t cmd_packet[PACKET_SIZE] = {0};

    // Prepare the command packet
    cmd_packet[0] = SPI_CMD(cmd);
    cmd_packet[1] = (arg >> 24);
    cmd_packet[2] = (arg >> 16);
    cmd_packet[3] = (arg >> 8);
    cmd_packet[4] = (arg >> 0);

    /*if (config->enable_crc) {
        uint8_t crc = _crc7(cmd_packet, 5);
        cmd_packet[5] = (crc << 1) | 0x01;
    } else*/ {
        switch (cmd) {
        case CMD0_GO_IDLE_STATE:
            cmd_packet[5] = 0x95;
            break;
        case CMD8_SEND_IF_COND:
            cmd_packet[5] = 0x87;
            break;
        default:
            cmd_packet[5] = 0xFF;    // Make sure bit 0-End bit is high
            break;
        }
    }

    // send a command
    spi_write_blocking(m_spi_inst, cmd_packet, PACKET_SIZE);

    // The received byte immediataly following CMD12 is a stuff byte,
    // it should be discarded before receive the response of the CMD12.
    if (cmd == CMD12_STOP_TRANSMISSION)
        spi_write_blocking(m_spi_inst, &SPI_FILL_CHAR, 1);

    // Loop for response: Response is sent back within command response time (NCR), 0 to 8 bytes for SDC
    for (int i = 0; i < 16; i++) {
        spi_write_read_blocking(m_spi_inst, &SPI_FILL_CHAR, &response, 1);
        if (!(response & R1_RESPONSE_RECV)) {
            break;
        }
    }
    return response;
}

int storage_pico_sdcard::_cmd(uint8_t cmd, uint32_t arg, bool is_acmd, uint32_t *resp) {
    int32_t status = 0;
    uint32_t response;

    _preclock_then_select();
    // No need to wait for card to be ready when sending the stop command
    if (cmd != CMD12_STOP_TRANSMISSION) {
        if (!_wait_ready()) {
            printf("Card not ready yet \n");
        }
    }

    // Re-try command
    for (int i = 0; i < 3; i++) {
        // Send CMD55 for APP command first
        if (is_acmd) {
            response = _cmd_spi(CMD55_APP_CMD, false);
            // Wait for card to be ready after CMD55
            if (!_wait_ready())
                printf("Card not ready yet (retry)\n");
        }

        // Send command over SPI interface
        response = _cmd_spi(cmd, arg);
        if (response == R1_NO_RESPONSE) {
            printf("No response CMD:%d \n", cmd);
            continue;
        }
        break;
    }

    // Pass the response to the command call if required
    if (resp)
        *resp = response;

    // Process the response R1  : Exit on CRC/Illegal command error/No response
    if (response == R1_NO_RESPONSE) {
        _postclock_then_deselect();
        printf("No response CMD:%d response: 0x%x\n", cmd, response);
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;         // No device
    }
    if (response & R1_COM_CRC_ERROR) {
        _postclock_then_deselect();
        printf("CRC error CMD:%d response 0x%x\n", cmd, response);
        return SD_BLOCK_DEVICE_ERROR_CRC;                // CRC error
    }
    if (response & R1_ILLEGAL_COMMAND) {
        _postclock_then_deselect();
        // printf("Illegal command CMD:%d response 0x%x\n", cmd, response);
        //if (cmd == CMD8_SEND_IF_COND) {                  // Illegal command is for Ver1 or not SD Card
        //    config->card_type = CARD_UNKNOWN;
        //}
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;      // Command not supported
    }

    // printf("CMD:%d \t arg:0x%x \t Response:0x%x\n", cmd, arg, response);
    // Set status for other errors
    if ((response & R1_ERASE_RESET) || (response & R1_ERASE_SEQUENCE_ERROR))
        status = SD_BLOCK_DEVICE_ERROR_ERASE;            // Erase error
    else if ((response & R1_ADDRESS_ERROR) || (response & R1_PARAMETER_ERROR))
        // Misaligned address / invalid address block length
        status = SD_BLOCK_DEVICE_ERROR_PARAMETER;

    // Get rest of the response part for other commands
    switch (cmd) {
        case CMD8_SEND_IF_COND:             // Response R7
            //printf("V2-Version Card\n");
            m_cardType = SDCARD_V2; // fallthrough
        // Note: No break here, need to read rest of the response
        case CMD58_READ_OCR:                // Response R3
            response  = (_spi_write_read() << 24);
            response |= (_spi_write_read() << 16);
            response |= (_spi_write_read() << 8);
            response |= _spi_write_read();
            //printf("R3/R7: 0x%x\n", response);
            break;

        case CMD12_STOP_TRANSMISSION:       // Response R1b
        case CMD38_ERASE:
            _wait_ready();
            break;

        case ACMD13_SD_STATUS:             // Response R2
            response = _spi_write_read();
            //printf("R2: 0x%x\n", response);
            break;

        default:                            // Response R1
            break;
    }

    // Pass the updated response to the command
    if (resp)
        *resp = response;

    // Do not deselect card if read is in progress.
    if (((CMD9_SEND_CSD == cmd) || (ACMD22_SEND_NUM_WR_BLOCKS == cmd) ||
            (CMD24_WRITE_BLOCK == cmd) || (CMD25_WRITE_MULTIPLE_BLOCK == cmd) ||
            (CMD17_READ_SINGLE_BLOCK == cmd) || (CMD18_READ_MULTIPLE_BLOCK == cmd))
            && (!status)) {
        return SD_BLOCK_DEVICE_ERROR_OK;
    }
    // Deselect card
    _postclock_then_deselect();
    return status;
}

uint32_t bit_extract(uint8_t csd[16],uint8_t msb,uint8_t lsb) {
    uint32_t result = 0;
    for (; msb>=lsb; --msb)
        result = (result << 1) | ((csd[15 - (msb>>3)] >> (msb & 7)) & 1);
    return result;
}

storage_pico_sdcard* storage_pico_sdcard::create(uint8_t spi,
		uint8_t sd_sclk_pin,uint8_t sd_mosi_pin,uint8_t sd_miso_pin,
		uint8_t sd_cs_pin,uint8_t sd_det_pin) {
    
    storage_pico_sdcard *result = new storage_pico_sdcard;
    struct spi_inst *spi_inst = SPI_INSTANCE(spi);
    result->m_spi_inst = spi_inst;
    result->m_sclk_pin = sd_sclk_pin;
    result->m_mosi_pin = sd_mosi_pin;
    result->m_miso_pin = sd_miso_pin;
    result->m_cs_pin = sd_cs_pin;
    result->m_det_pin = sd_det_pin;
    result->m_cardType = SDCARD_NONE;

    gpio_set_function(sd_mosi_pin, GPIO_FUNC_SPI);
    gpio_set_function(sd_miso_pin, GPIO_FUNC_SPI);
    gpio_set_function(sd_sclk_pin, GPIO_FUNC_SPI);
    gpio_init(sd_cs_pin);
    gpio_set_dir(sd_cs_pin, GPIO_OUT);
    gpio_pull_up(sd_miso_pin);
    gpio_set_drive_strength(sd_mosi_pin, GPIO_DRIVE_STRENGTH_4MA);
    gpio_set_drive_strength(sd_sclk_pin, GPIO_DRIVE_STRENGTH_4MA);
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

    int err = result->_cmd(CMD8_SEND_IF_COND,0x1AA,false,&response);
    if (err && err != SD_BLOCK_DEVICE_ERROR_UNSUPPORTED) {
        printf("cmd8 failed\n");
        delete result;
        return nullptr;
    }

    err = result->_cmd(CMD58_READ_OCR, 0, false,&response);
    if (err) {
        printf("READ_OCR failed\n");
        delete result;
        return nullptr;
    }
    if (!(response & OCR_3_3V)) {
        printf("cannot use 3.3V\n");
        result->m_cardType = SDCARD_UNKNOWN;
        delete result;
        return nullptr;
    }
    uint32_t arg = result->m_cardType==SDCARD_V2? OCR_HCS_CCS : 0;
    do {
        err = result->_cmd(ACMD41_SD_SEND_OP_COND, arg, true, &response);
    } while (response & R1_IDLE_STATE);

    if (!err && result->m_cardType==SDCARD_V2) {
        err = result->_cmd(CMD58_READ_OCR, 0, false, &response);
        if (response & OCR_HCS_CCS)
            result->m_cardType = SDCARD_V2HC;
        //printf("V2 card\n");
    }
    else
        result->m_cardType = SDCARD_V1;
        
    err = result->_cmd(CMD9_SEND_CSD, 0, false, nullptr);
    if (err) {
        printf("error in reading size\n");
        delete result;
        return nullptr;
    }
    uint8_t csd[16] = {0}, cs[2];
    if (!result->_wait_token(0xFE)) {
        printf("wait_token timed out\n");
        delete result;
        return nullptr;
    }
    spi_read_blocking(result->m_spi_inst,0xFF,csd,16);
    spi_read_blocking(result->m_spi_inst,0xFF,cs,2);
    uint32_t READ_BL_LEN = 0, C_SIZE = 0, C_SIZE_MULT = 0;
    uint32_t BLOCKNR = 0;
    //printf("CSD - %02x %02x %02x %02x  .... %02x\n",csd[0],csd[1],csd[2],csd[3],csd[15]);
    switch (bit_extract(csd,127,126)) {
        case 0:
            READ_BL_LEN = bit_extract(csd,83,80);
            C_SIZE = bit_extract(csd,73,62);
            C_SIZE_MULT = bit_extract(csd,49,47);
            BLOCKNR = (C_SIZE+1) << (C_SIZE_MULT + 2 + READ_BL_LEN - 9);
            break;
        case 1:
            BLOCKNR = (bit_extract(csd,69,48) + 1) << 10;
            break;
        case 2:
            BLOCKNR = (bit_extract(csd,75,48) + 1) << 10;
            break;
    }
    static const char *cards[] = { "none", "v1", "v2", "v2_hc", "unknown" };
    printf("sdcard - created %s, %u blocks %uk\n",cards[result->m_cardType],BLOCKNR,(BLOCKNR >> 1));
    result->m_blockCount = BLOCKNR;
    result->_postclock_then_deselect();
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
    return m_blockCount;
}

bool storage_pico_sdcard::readBlock(size_t index,void *dest) {
    if (m_cardType != SDCARD_V2HC)
        index <<= 9;
    if (_cmd(CMD17_READ_SINGLE_BLOCK, (uint32_t)index,false,nullptr)) {
        printf("readBlock - cmd failed");
        return false;
    }
    if (!_wait_token(0xFE)) {
        printf("readBlock - wait timeout\n");
        return false;
    }
    spi_read_blocking(m_spi_inst,0xFF,(uint8_t*)dest,512);
    uint8_t cs[2];
    spi_read_blocking(m_spi_inst,0xFF,cs,2);
    // printf("readBlock - %02x ...\n",*(uint8_t*)dest);
    _postclock_then_deselect();
    return true;
}

bool storage_pico_sdcard::writeBlock(size_t index,const void *src) {
    if (m_cardType != SDCARD_V2HC)
        index <<= 9;
    if (_cmd(CMD24_WRITE_BLOCK, (uint32_t)index,false,nullptr)) {
        printf("readBlock - cmd failed");
        return false;
    }

    // indicate start of block
    _spi_write_read(0xFE);

    // write the data
    spi_write_blocking(m_spi_inst, (uint8_t*)src, 512);

    uint16_t crc = 0;
    /* if (config->enable_crc) {
        crc = _crc16(buffer, length);
    } */

    // write the checksum CRC-16
    _spi_write_read(crc >> 8);
    _spi_write_read(crc);

    // check the response token
    uint8_t response = _spi_write_read(SPI_FILL_CHAR);
    //printf("write response = %x\n",response);
    bool result = (response & 0x1F) != 5 || !_wait_ready();
    _postclock_then_deselect();
    return result;
}

const void *storage_pico_sdcard::memoryMap(size_t index,size_t blockCount) {
    return nullptr;    
}

}
