#include "storage_pico_flash.h"

#include <string.h>
#include <hardware/flash.h>

namespace hal {

// Flash must be erased FLASH_SECTOR_SIZE bytes at a time (4096b)
// It can be programmed FLASH_PAGE_SIZE bytes at a time (256b)
// We expose a sector size of 512 for compatibility with SD cards.
// Erasing flash sets it to all 1's. When you program flash it
// can only reset a bit to 0; if you want to set it back to one you
// have to erase the entire page first.
const size_t flash_offset = 1024 * 1024;
const size_t flash_size = 2 * 1024 * 1024;
const size_t flash_sector_count = (flash_size - flash_offset) >> 9;

uint8_t scratch[FLASH_SECTOR_SIZE];
size_t currentSector;
bool dirty;

void storage_pico_flash::init() {
    currentSector = ~0;
    dirty = false;
}

size_t storage_pico_flash::getBlockSize() const {
    return 512;
}

size_t storage_pico_flash::getBlockCount() const { 
    return flash_sector_count;
}

inline size_t s2o(size_t s) {
    return flash_offset + (s << 9); 
}

inline void *s2a(size_t s) {
    return (char*)XIP_BASE + s2o(s);
}

void storage_pico_flash::flush() {
    if (dirty) {
        flash_range_erase(s2o(currentSector),sizeof(scratch));
        flash_range_program(s2o(currentSector),scratch,sizeof(scratch));
        dirty = false;
    }
}

const size_t sectorMask = ((FLASH_SECTOR_SIZE/512)-1);

bool storage_pico_flash::writeBlock(size_t index,const void *data) {
    if (index >= flash_sector_count)
        return false;

    // if we're writing to a different page than is in the cache,
    // flush the old page and bring in the new one.
    size_t thisSector = index & ~sectorMask;
    if (thisSector != currentSector) {
        flush();
        currentSector = thisSector;
        memcpy(scratch,s2a(thisSector),FLASH_PAGE_SIZE);
    }
    memcpy(scratch + ((index & sectorMask) << 9),data,512);
    dirty = true;
    return true;
}

bool storage_pico_flash::readBlock(size_t index,void *data) {
    if (index >= flash_sector_count)
        return false;
    
    // read from (potentially unflushed) cache if it was just written
    if ((index & ~sectorMask) == (currentSector & ~sectorMask))
        memcpy(data, scratch + ((index & sectorMask) << 9),512);
    else
        memcpy(data,s2a(index),512);
    return true;
}

const void* storage_pico_flash::memoryMap(size_t index,size_t /*blockCount*/) {
    return s2a(index);
}

storage* storage::create(const char*) {
    auto result = new storage_pico_flash;
    result->init();
    return result;
}

}
