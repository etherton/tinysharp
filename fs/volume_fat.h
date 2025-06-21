#pragma once

#include "volume.h"

namespace fs {

class volumeFat: public volume {
public:
    bool init(hal::storage *s);
    static volumeFat* create(hal::storage*);

	virtual bool openDir(directory &handle,directoryEntry const * const de);
	virtual bool readDir(directory &handle,directoryEntry &dest);
    virtual bool locateEntry(directoryEntry &dest,const char *path);
    virtual uint32_t readFile(const directoryEntry &de,void *dest,uint32_t offset,uint32_t size);

private:
    uint8_t *getSector(int32_t,uint8_t = 0);
    hal::storage *m_storage;
    int32_t m_rootDirectorySectorOrCluster; // if negative, it's a cluster (2+); if non-negative, raw sector.
    uint32_t m_totalSectors;
    uint32_t m_cluster2;
    uint32_t m_sectorsPerFat;
    uint16_t m_sectorsPerCluster, m_reservedSectors, m_rootDirectoryCount /*nonzero for fat16*/;
    uint8_t m_numberOfFatCopies;
    static const uint32_t kCacheSize = 4;
    uint32_t m_cacheSectors[kCacheSize];
    uint8_t m_cacheAge[kCacheSize];
    uint8_t m_cacheBuffers[kCacheSize][512];
};

}


