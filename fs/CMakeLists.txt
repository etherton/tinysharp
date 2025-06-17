add_library(fs
	volume.cpp volume.h
	volume_fat.cpp volume_fat.h
	fat_structs.h
	mbr.h
	types.h
	)

include_directories(
	..
	)
