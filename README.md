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

Next, configure cmake.  This is a system dependent activity but can look like the following:

   
