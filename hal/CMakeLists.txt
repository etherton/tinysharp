add_library(hal INTERFACE)

target_sources(hal INTERFACE
	video.cpp video.h
	video_pico.cpp video_pico.h
	keyboard.cpp keyboard.h
	keyboard_pico.cpp keyboard_pico.h
	storage.cpp storage.h
	storage_pico_flash.cpp storage_pico_flash.h
	timer.cpp timer.h
	)

target_link_libraries(hal INTERFACE
	pico_stdlib 
	hardware_spi
	hardware_i2c
	)

target_include_directories(hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

