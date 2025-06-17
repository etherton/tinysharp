#include "volume_fat.h"
#include "hal/storage.h"
#include "hal/storage_inner.h"
#include "mbr.h"
#include "fat_structs.h"
#include <stdio.h>

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

bool volumeFat::init(hal::storage *s) {
    bootSector b;
    if (!s->readBlock(0,&b)) {
        printf("unable to read boot sector\n");
        return false;
    }
    printf("%u reserved sectors, %u fat copies, %u sectors per fat, poss. root entries %u\n",
        b.reservedSectors.get(), b.numberOfFatCopies, b.sectorsPerFat.get()?b.sectorsPerFat.get():b.fat32.logicalSectorsPerFat.get(), b.numberOfPossRootEntries.get());
    uint32_t cluster_2 = b.getCluster2();
    printf("cluster 2 is at sector %u\n",cluster_2);
    return true;
}

}

