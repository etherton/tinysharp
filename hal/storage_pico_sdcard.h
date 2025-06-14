#pragma once

#include "storage.h"

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

	static storage_pico_sdcard *create(uint8_t spi,
		uint8_t sd_clk_pin = 18,
		uint8_t sd_mosi_pin = 19,
		uint8_t sd_miso_pin = 16,
		uint8_t sd_cs_pin = 17,
		uint8_t sd_det_pin = 22);
};

} // namespace hal
