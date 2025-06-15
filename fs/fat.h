#pragma once

#include "types.h"

namespace fs {

struct bootSector {
	uint8_t jmp[3];			// 0x000
	char osName[8];			// 0x003
	word bytesPerSector;		// 0x00B - usually 512
	uint8_t sectorsPerCluster;	// 0x00D
	word reservedSectors;		// 0x00E
	uint8_t numberOfFatCopies;	// 0x010
	word numberOfPossRootEntries;	// 0x011 (zero for FAT32)
	word smallNumberOfSectors;	// 0x013 - used when fits in 16 bits, else zero
	uint8_t mediaDescriptor;	// 0x015 - 0xF8, "fixed disk"
	word sectorsPerFat;			// 0x016 (only in FAT16)
	word sectorsPerTrack;		// 0x018
	word numberOfHeads;		// 0x01A
	dword hiddenSectors;		// 0x01C
	dword largeNumberOfSectors;	// 0x020 - used when larger then 16 bits, else zero
	union {
		struct {
			uint8_t driveNumber;		// 0x024
			uint8_t reserved;		// 0x025 - if nonzero, needs integrity check
			uint8_t extendedBootSignature;	// 0x026 - should contain 0x29 if it's this variant
			dword volumeSerialNumber;	// 0x027
			char volumeLabel[11];		// 0x02B
			char filesystemType[8];		// 0x036
			char bootstrapCode[448];	// 0x03E
		} fat16;
		struct {
			dword logicalSectorsPerFat;	// 0x024
			word mirroringFlags; // 0x028
			word version; 		// 0x02A - should be zero
			dword rootDirectoryStart; // 0x02C
			word infoSectorLsn; // 0x030
			word fatSectorCopyLsn; // 0x032
			char reservedFilename[12]; // 0x034
			uint8_t driveNumber; // 0x040
			uint8_t reserved; // 0x041
			uint8_t extendedBootSignature; // 0x042 - should contain 0x29 if it's this variant
			dword volumeSerialNumber; // 0x043
			char volumeLabel[11]; // 0x47 (not available if extendedBootSignature is 0x28)
			char filesystemType[8]; // 0x052
			char bootstrapCode[1];
		} fat32;
	};
	bool isFat16() const { return fat16.extendedBootSignature == 0x29; }
	bool isFat32() const { return !isFat16() && fat32.extendedBootSignature == 0x29; }
	const char *getVolumeLabel() const {
		return isFat16()? fat16.volumeLabel : isFat32()? fat32.volumeLabel : "none";
	}
	word signature;			// 0x1FE -- 0xAA55
};

static_assert(sizeof(bootSector) == 512);

struct fat12_table {
	uint8_t packed[512];
	uint16_t get(uint16_t index) const {
		return index & 1
			? (packed[index * 3 + 1] >> 4) | (packed[index * 3 + 2] << 4)
			: packed[index * 3] | ((packed[index * 3 + 1] & 0xF) << 8);
	}
};

struct fat16_table {
	uint16_t table[256];
	uint16_t get(uint16_t index) const {
		return table[index];
	}
};

struct fat32_table {
	uint32_t table[128];
	uint32_t get(uint16_t index) const {
		return table[index];
	}
};

enum class fat12: uint16_t {
	free = 0x000,
	nextClusterMin = 0x001,
	nextClusterMax = 0xFEF,
	reservedClusterMin = 0xFF0,
	reservedClusterMax = 0xFF6,
	badCluster = 0xFF7,
	eofMin = 0xFF8,
	eofMax = 0xFFF
};

enum class fat16: uint16_t {
	free = 0x0000,
	nextClusterMin = 0x0003,
	nextCluserMax = 0xFFEF,
	badClusterMin = 0xFFF7,
	badClusterMax = 0xFFF7,
	eofMin = 0xFFF8,
	eofMax = 0xFFFF
};

enum class fat32: uint32_t {
	free = 0x0,
	nextClusterMin = 0x1,
	nextClusterMax = 0xFFFF'FFF5,
	badClusterMin = 0xFFFF'FFF6,
	badClusterMax = 0xFFFF'FFF7,
	eof = 0xFFFF'FFFF
};

struct time_t {
	uint16_t seconds: 5, minutes: 6, hours: 5;
};

struct date_t {
	uint16_t day: 5, month: 4, year: 7; // day/month are 1-based, year starts at 1980
};

struct dirEntry {
	char filename[8]; 		// 0x00 - 0xE5 means freed, 0x00 means freed and all past are freed.
	char ext[3];			// 0x08
	uint8_t attributes;		// 0x0B - see attributes_t
	uint8_t reserved;		// 0x0C
	uint8_t creationMs;		// 0x0D
	word creationTime;		// 0x0E
	word creationDate;		// 0x10
	word lastAccess;		// 0x12
	word startingClusterHi;		// 0x14
	word lastWriteTime;		// 0x16
	word lastWriteDate;		// 0x18
	word startingClusterLo;		// 0x1A
	dword fileSizeBytes;		// 0x1C
};

// lfnEntry's are stored just BEFORE the matching legacy entry, in descending order (highest ordinal first)
struct lfnEntry {
	uint8_t ordinalField;		// 0x00 - 01, 02, etc with bit 6 set in last one
	word unicode_1_5[5];		// 0x01
	uint8_t attributes;		// 0x0B
	uint8_t reservedZeroByte;	// 0x0C
	uint8_t checksum;		// 0x0D - value = rotRight(value) + value (of the 8.3 name, incl. spaces)
	word unicode_6_11[5];		// 0x0E
	word reservedZeroWord;		// 0x1A
	word unicode_12_13[2];		// 0x1C
};

enum class attributes_t: uint8_t  {
	readOnly = 1,
	hidden = 2,
	system = 4,
	volume = 8,
	lfn = 15,
	directory = 16,
	archive = 32,
};

} // namespace fs
