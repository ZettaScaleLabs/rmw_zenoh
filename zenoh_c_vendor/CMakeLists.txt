cmake_minimum_required(VERSION 3.16)
project(zenoh_c_vendor)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)

# Include the ExternalProject module
include(ExternalProject)

# Define installation directory
set(ZENOHC_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})

# Add the external project
ExternalProject_Add(zenohc
  PREFIX ${CMAKE_BINARY_DIR}/zenohc
  GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-c.git
  GIT_TAG dev/1.0.0
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${ZENOHC_INSTALL_DIR}
  BUILD_IN_SOURCE 1
)

# Create a custom target to manage dependencies
add_custom_target(zenoh_c_vendor ALL
  DEPENDS zenohc
)

# Add the installation directory to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH ${ZENOHC_INSTALL_DIR})

ament_package()
