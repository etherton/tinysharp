#include "volume_fat.h"
#include "hal/storage.h"
#include "hal/storage_inner.h"
#include "mbr.h"
#include "fat_structs.h"
#include <stdio.h>
#include <string.h>

namespace fs {

volumeFat* volumeFat::create(hal::storage *s) {
    mbr m;
    if (s->readBlock(0,&m) && m.signature.get() == 0xaa55) {
        for (int i=0; i<4; i++) {
            switch (m.partitions[i].type) {
                case partitionType::fat12:
                    printf("do not support fat12, reformat to fat16\n");
                    break;
                default:
                    printf("unknown partition type %u\n",m.partitions[i].type);
                    break;
                case partitionType::fat16_smaller32M:
                case partitionType::fat16_larger32M:
                case partitionType::fat16_lba:
                case partitionType::fat32_lba:
                    volumeFat *v = new volumeFat;
                    if (v->init(new hal::storage_inner(s,m.partitions[i].lba.get(),m.partitions[i].sizeInSectors.get())))
                        return v;
                    else {
                        delete v;
                        return nullptr;
                    }
            }
        }
        return nullptr;
    }
    else {
        printf("unable to read storage\n");
        return nullptr;
    }
}

uint8_t *volumeFat::getSector(int32_t sector,uint8_t offset) {
    if (sector < 0)
        sector = m_cluster2 + ((-sector - 2) * m_sectorsPerCluster) + offset;
    uint32_t i;
    for (i=0; i<kCacheSize; i++) {
        if (m_cacheSectors[i] == sector)
            break;
    }
    if (i==kCacheSize) {
        uint8_t oldest = m_cacheSectors[0];
        uint32_t oldestIndex = 0;
        for (i=1; i<kCacheSize; i++) {
            if (m_cacheAge[i] > oldest) {
                oldest = m_cacheAge[i];
                oldestIndex = i;
            }
        }
        i = oldestIndex;
        if (!m_storage->readBlock(sector,m_cacheBuffers[i]))
            return nullptr;
        m_cacheSectors[i] = sector;
    }
    // current slot is newest, age all the other slots
    m_cacheAge[i] = 0;
    for (uint32_t j=1; j<kCacheSize; j++) {
        auto &age = m_cacheAge[(i + j) % kCacheSize];
        if (age != 0xFF)
            ++age;
    }
    return m_cacheBuffers[i];
}
bool volumeFat::init(hal::storage *s) {
    for (uint32_t i=0; i<kCacheSize; i++) {
        m_cacheSectors[i] = ~0U;
        m_cacheAge[i] = 0;
    }
    m_storage = s;
    bootSector &b = *(bootSector*)getSector(0);
    printf("%u reserved sectors, %u fat copies, %u sectors per fat, poss. root entries %u\n",
        b.reservedSectors.get(), b.numberOfFatCopies, b.sectorsPerFat.get()?b.sectorsPerFat.get():b.fat32.logicalSectorsPerFat.get(), b.numberOfPossRootEntries.get());
    m_sectorsPerCluster = b.sectorsPerCluster;
    m_numberOfFatCopies = b.numberOfFatCopies;
    m_sectorsPerFat = b.sectorsPerFat.get()? b.sectorsPerFat.get() : b.fat32.logicalSectorsPerFat.get();
    m_totalSectors = b.smallNumberOfSectors.get()? b.smallNumberOfSectors.get() : b.largeNumberOfSectors.get();
    m_reservedSectors = b.reservedSectors.get();
    m_cluster2 = b.getCluster2();
    m_rootDirectorySectorOrCluster = b.isFat16()? m_reservedSectors + m_sectorsPerFat * m_numberOfFatCopies : -(int32_t)b.fat32.rootDirectoryStart.get();
    m_rootDirectoryCount = b.numberOfPossRootEntries.get(); // 0 for fat32

    return true;
}


bool volumeFat::openDir(directory &d,directoryEntry const * const de) {
    d.currentEntry = 0;
    if (!de) {
        d.currentSectorOrCluster = m_rootDirectorySectorOrCluster;
        d.maxEntries = m_rootDirectoryCount? m_rootDirectoryCount : (m_sectorsPerCluster << 4);
    }
    else if (de->directory) {
        d.currentSectorOrCluster = -de->firstCluster;
        d.maxEntries = m_sectorsPerCluster << 4; // 16 per sector
    }
    else
        return false;
    return true;
}

bool volumeFat::readDir(directory &d,directoryEntry &de) {
    memset(de.filename,0,sizeof(de.filename));
    for (;d.currentEntry<d.maxEntries;d.currentEntry++) {
        if (d.maxEntries && d.currentEntry >= d.maxEntries)
            return false;
        dirEntry *b = (dirEntry*) getSector(d.currentSectorOrCluster,d.currentEntry >> 4) + (d.currentEntry & 15);
        if ((b->attributes & 15) == 15) {
            lfnEntry *le = (lfnEntry*) b;
            uint8_t index = ((le->ordinalField & 63)-1) * 13;
            de.filename[index+0] = le->unicode_1_5[0].lo;
            de.filename[index+1] = le->unicode_1_5[1].lo;
            de.filename[index+2] = le->unicode_1_5[2].lo;
            de.filename[index+3] = le->unicode_1_5[3].lo;
            de.filename[index+4] = le->unicode_1_5[4].lo;
            de.filename[index+5] = le->unicode_6_11[0].lo;
            de.filename[index+6] = le->unicode_6_11[1].lo;
            de.filename[index+7] = le->unicode_6_11[2].lo;
            de.filename[index+8] = le->unicode_6_11[3].lo;
            de.filename[index+9] = le->unicode_6_11[4].lo;
            de.filename[index+10] = le->unicode_6_11[5].lo;
            de.filename[index+11] = le->unicode_12_13[0].lo;
            de.filename[index+12] = le->unicode_12_13[1].lo;
            // for (uint8_t i=0; i<13; i++) printf("%c",de.filename[index+i]);
            // printf(" - lfn found index %u\n",index);
        }
        else if (!b->filename[0])
            return false;
        else if (!(b->attributes & 0x8) && b->filename[0] != 0xE5) {
            if (!de.filename[0]) {
                strncpy(de.filename,b->filename,8);
                uint8_t len = 8;
                while (len && de.filename[len-1]==32)
                    de.filename[--len] = 0;
                if (b->ext[0]!=32) {
                    de.filename[len++] = '.';
                    strncpy(de.filename+len,b->ext,3);
                    uint8_t len2 = 3;
                    while (len2 && de.filename[len+len2-1]==32)
                        de.filename[len+--len2] = 0;
                }
            }
            de.readOnly = !!(b->attributes & (uint8_t)attributes_t::readOnly);
            de.hidden = !!(b->attributes & (uint8_t)attributes_t::hidden);
            de.system = !!(b->attributes & (uint8_t)attributes_t::system);
            de.volume = !!(b->attributes & (uint8_t)attributes_t::volume);
            de.directory = !!(b->attributes & (uint8_t)attributes_t::directory);
            de.firstCluster = m_rootDirectoryCount? b->startingClusterLo.get() :
                b->startingClusterLo.get() | (b->startingClusterHi.get() << 16);
            de.size = b->fileSizeBytes.get();
            d.currentEntry++;
            return true;
        }
    }
    return false;
}

}

