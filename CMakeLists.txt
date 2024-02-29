cmake_minimum_required(VERSION 3.16)
project(deduplicator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
include(CheckCXXCompilerFlag)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(Boost COMPONENTS program_options REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(spdlog REQUIRED)
find_package(Earthworm REQUIRED)

# Versioning information
configure_file(${CMAKE_SOURCE_DIR}/src/version.hpp.in
               ${CMAKE_SOURCE_DIR}/src/version.hpp)

add_executable(deduplicator src/main.cpp src/traceBuf2.cpp src/waveRing.cpp)
set_target_properties(deduplicator PROPERTIES
                      CXX_STANDARD 20
                      CXX_STANDARD_REQUIRED YES 
                      CXX_EXTENSIONS NO) 
target_include_directories(deduplicator PRIVATE ${CMAKE_SOURCE_DIR}/include Boost::program_options ${Earthworm_INCLUDE_DIR})
target_link_libraries(deduplicator PRIVATE ${Earthworm_MT_LIBRARY} ${Earthworm_UTIL_LIBRARY} Boost::program_options)

include(GNUInstallDirs)
install(TARGETS deduplicator
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
