#pragma once

#include "storage.h"

struct spi_inst;

namespace hal {

class storage_pico_sdcard: public storage {
public:
	void init();
	void flush();
	size_t getBlockSize() const;
	size_t getBlockCount() const;
	bool readBlock(size_t index,void *dest);
	bool writeBlock(size_t index,const void *dest);
	const void *memoryMap(size_t index,size_t blockCount);

	static storage_pico_sdcard *create(
		uint8_t spi = 0,
		uint8_t sd_sclk_pin = 18,
		uint8_t sd_mosi_pin = 19,
		uint8_t sd_miso_pin = 16,
		uint8_t sd_cs_pin = 17,
		uint8_t sd_det_pin = 22);
private:
	struct spi_inst *m_spi_inst;
	uint8_t m_sclk_pin, m_mosi_pin, m_miso_pin, m_cs_pin, m_det_pin;
	uint8_t m_cardType;
	uint32_t m_blockCount;
	inline void _preclock_then_select();
	inline void _postclock_then_deselect();
	inline uint8_t _spi_write_read(uint8_t value = 0xFF);
	int _cmd(uint8_t cmd, uint32_t arg, bool is_acmd, uint32_t *outOptResp);
	uint8_t _cmd_spi(uint8_t cmd,uint32_t arg);
	bool _wait_token(uint8_t token,uint32_t timeout = 300);
	inline bool _wait_ready() { return _wait_token(0xFF, 5000); }
};

} // namespace hal
