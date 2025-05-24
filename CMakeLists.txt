cmake_minimum_required(VERSION 3.13...3.27)

include($ENV{PICO_SDK_PATH}/pico_sdk_import.cmake)
project(tinysharp C CXX ASM)

### set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR} CACHE PATH "Install prefix set to project root" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

set(PICO_BOARD pico)
add_subdirectory(drivers)

pico_sdk_init()

add_executable(test)
target_link_libraries(test)
    pico_stdlib
    hardware_sync
    hardware_flash
    hardware_irq
    hardware_adc
    hardware_pwm
    hardware_i2c
    hardware_spi
    hardware_dma
    hardware_exception
    hardware_pio
    pico_multicore
    drivers
  )

  ## pico_enable_stdio_usb(picocalc_sd_boot_${board_name} 0)
  ## pico_enable_stdio_uart(picocalc_sd_boot_${board_name} 1)

  ## pico_add_extra_outputs(picocalc_sd_boot_${board_name})

  ## target_link_options(picocalc_sd_boot_${board_name} PRIVATE -Wl,--print-memory-usage)

  # Define the output directory relative to the project root directory
  set(output_dir prebuild_output/${board_name})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/picocalc_sd_boot_${board_name}.elf
    DESTINATION ${output_dir}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/picocalc_sd_boot_${board_name}.uf2
    DESTINATION ${output_dir}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/picocalc_sd_boot_${board_name}.bin
    DESTINATION ${output_dir}
  )
