add_library(hal INTERFACE)

target_sources(hal INTERFACE
	video.cpp video.h
	video_picocalc.cpp video_picocalc.h
	keyboard.cpp keyboard.h
	keyboard_picocalc.cpp keyboard_picocalc.h
	storage.cpp storage.h
	storage_inner.h
	storage_pico_flash.cpp storage_pico_flash.h
	storage_pico_sdcard.cpp storage_pico_sdcard.h
	timer.cpp timer.h
	)

target_link_libraries(hal INTERFACE
	pico_stdlib 
	hardware_spi
	hardware_i2c
	)

target_include_directories(hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

