# About

The data deuplicator copies from packets from one ring to another ring and in the process can do the following:

  1.  Not pass duplicate packets.
  2.  Not pass old data.
  3.  Not pass data from the future.

# Compilation

To compile the code first download and build the prereuisites.  

## Prerequisites

The following prerequisites must be installed prior to compiling the software:

  1. A C++20 compliant compiler.
  2. [CMake](https://cmake.org/) for generation of a Makefile.
  3. [Earthworm](http://folkworm.ceri.memphis.edu/ew-dist/).  
  4. [spdlog](https://github.com/gabime/spdlog) for logging.
  5. [Boost](https://www.boost.org/) which implements the circular buffer.

## Getting the Code

After building and installing the prerequisites download the software

   git clone https://github.com/uofuseismo/deduplicator.git

## Configuring CMake

Next, configure cmake.  If everything is in a standard location then this will be dead easy.  But, let's say libboost is in a strange spot, then a configuration script could look like this:

    #!/usr/bin/bash
    export CXX=/usr/bin/g++
    export Boost_ROOT=${HOME}/boost_1_84_0/
    BUILD_DIR=./build
    if [ -d ${BUILD_DIR} ]; then
       rm -rf ${BUILD_DIR}
    fi
    mkdir -p ${BUILD_DIR}
    cd ${BUILD_DIR}
    cmake .. \
    -DCMAKE_CXX_COMPILER=${CXX}

## Building the Code

Assuming the CMake configuration was successful descend into the build directory, e.g.,

    cd build
    make

## Installing the Code

Provided the build was successful, you can install the executable (which by default will go to /usr/local/bin)

   sudo make install

   

   
