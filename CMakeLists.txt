cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")  
find_package(LTTng REQUIRED)
find_package(OPENTRACE REQUIRED)
find_package(JAEGER REQUIRED)
find_package(YAMLCPP REQUIRED)

# make && make test
enable_testing()

add_subdirectory(blkin-lib)
