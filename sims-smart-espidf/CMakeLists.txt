# SIMS-SMART ESP-IDF Project
# Voice-controlled incident reporting device

cmake_minimum_required(VERSION 3.16)

# Set the components search directories (only if directory exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components")
    set(EXTRA_COMPONENT_DIRS components)
endif()

# Include the ESP-IDF project configuration
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(sims-smart-espidf)
