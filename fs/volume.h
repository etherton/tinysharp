#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal { class storage; }

namespace fs {

struct directory {
	int32_t currentSectorOrCluster; // cluster if negative, else absolute sector.
	uint16_t currentEntry, maxEntries;
};

struct directoryEntry {
	char filename[119];
	bool readOnly:1, hidden:1, system:1, volume:1, directory:1;
	int32_t firstCluster;
	uint32_t size;
};

class file {

};

class volume {
public:
	// de==nullptr, open root directory, else open directory returned by readDir
	// must call readDir to obtain first directory entry if this returns true
	virtual bool openDir(directory &handle,directoryEntry const * const de) = 0;
	// returns next directory nentry, or false if no further
	virtual bool readDir(directory &handle,directoryEntry &dest) = 0;
    virtual bool locateEntry(directoryEntry &dest,const char *path) = 0;
    virtual uint32_t readFile(const directoryEntry &de,void *dest,uint32_t offset,uint32_t size) = 0;

};

extern volume *g_root;

};
