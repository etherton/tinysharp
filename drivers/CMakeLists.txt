add_library(drivers INTERFACE)

target_sources(drivers INTERFACE
	video.cpp video.h
	video_pico.cpp video_pico.h
	)

target_link_libraries(drivers INTERFACE
	pico_stdlib 
	hardware_spi
	)

target_include_directories(drivers INTERFACE ${CMAKE_CURRENT_LIST_DIR})

