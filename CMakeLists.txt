# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_KSCAN_74HC165)
  zephyr_library()
  zephyr_include_directories(drivers/include)
  zephyr_library_sources(drivers/kscan/kscan_74hc165.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_KSCAN_74HC165_ENCODER drivers/kscan/kscan_74hc165_encoder.c)
endif()
