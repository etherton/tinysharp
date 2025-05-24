# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

project(drivers
        VERSION 0.0.1
        DESCRIPTION "drivers for rp2040."
        )

add_library(lcdspi INTERFACE)

target_sources(lcdspi INTERFACE
        video.cpp video.h video_pico.cpp video_pico.h
        )

target_link_libraries(drivers INTERFACE  pico_stdlib hardware_spi)

target_include_directories(drivers INTERFACE ${CMAKE_CURRENT_LIST_DIR})
