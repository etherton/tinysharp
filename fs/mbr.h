#pragma once

#include "types.h"

namespace fs {

struct chs {
	uint8_t head, sector, cylinder;
};

enum class partitionType: uint8_t {
	unused,
	fat12,
	xenixRoot,
	xenixUser,
	fat16_smaller32M,
	extendedChs,
	fat16_larger32M,
	ntfs,

	aix,
	aixBootable,
	fat32_chs,
	fat32_lba, // 11
	d,
	fat16_lba,
	extendedPartition,
}; 

struct partition {
	uint8_t boot;	// 0x80 means bootable
	chs start;
	partitionType type; 
	chs end;
	dword lba;
	dword sizeInSectors;
};

struct mbr {
	uint8_t code[446];
	partition partitions[4];
	word signature; // 0xAA55
};

} // namespace fs
