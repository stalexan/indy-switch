# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(indy_switch)

target_add_binary_data(indy_switch.elf "main/elias_alexan_org.pem" TEXT)
target_add_binary_data(indy_switch.elf "main/initial_config.json" TEXT)
