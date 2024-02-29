# About

The data deduplicator copies from packets from one ring to another ring and in the process can do the following:

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

# Setting Up Earthworm

Now the executable is built you can use it in Earthworm.  To do this, first make sure there is a module identifier in the earthworm.d file.  For example:

    Module   MOD_DEDUPLICATOR      160

Next, make sure Earthworm's status manager knows about this application, so add its desc file (assuming this file is named deduplicator.desc) to the statmgr.d file

    Descriptor deduplicator.desc

Now, you can add this process as something to start/stop automatically.  To do this, in the startstop_unix.d file add the following

    Process          "deduplicator --ini=deduplicator.ini"
    Class/Priority   TS 0

where the executable is deduplicator (it is assumed this is in a path that Earthworm can see - e.g., /usr/local/bin) and the initialization file is called deduplicator.ini.

## The Desc File 

These don't need to be too fancy.  You can probably get by with something like:

    #
    #          Descriptor File for the deduplicator module
    #
    #    This file specifies when error messages are to be reported via
    #    email and pager.  All errors are logged in statmgr's daily log
    #    files.  The name of this file must be entered in statmgr's
    #    configuration file with a <Descriptor> command.
    #    (example:  Descriptor "arc2trig.desc" )
    #    The pager group name and a list of email recipients are listed
    #    in statmgr's configuration file.
    #
    #    Comment lines in this file are preceded by #.
    #
    #    Parameters:
    #
    #    <modName> is a text string included in each reported error message.
    #
    #    <modId> is the module id number as specified in the file
    #    earthworm.h.
    #
    #    <instId> is the installation id number as specified in the file
    #    earthworm.h.
    #
    #
    modName  deduplicator
    modId    MOD_DEDUPLICATOR
    instId   INST_UTAH
    #
    #
    #    Heartbeat Specification.  If the status manager does not receive
    #    a heartbeat message every <tsec> seconds from this module, an
    #    error will be reported (client module dead).  <page> is the maximum
    #    number of pager messages that will be reported and <mail> is the
    #    maximum number of email messages that will be reported.  If the
    #    page or mail limit is exceeded, no further errors will be reported
    #    until the status manager is restarted.
    #
    tsec: 60  page: 0  mail: 0
    #
    #
    # Uncomment the "restartMe" line to enable automatic restart of this
    # process by statmgr/startstop.  statmgr will issue a TYPE_RESTART message
    # for this process_id if it declares the patient dead.
    #
    restartMe

## The Initialization File

Next, we'll specify the initialization file.  Effectively, we will screen packets on the TEMP\_RING and pass `good' packets to the WAVE\_RING.

    # Module Identifier for this instace of deduplicator.  This should match
    # what is in earthworm.d.  The default is MOD_DEDUPLICATOR.
    moduleIdentifier=MOD_DEDUPLICATOR
    # Shared memory ring from which waveforms are read
    inputRingName=TEMP_RING
    # Shared memory ring to which waveforms are written
    outputRingName=WAVE_RING
    # Packets with ends time exceeding this many seconds from now into the future
    # will be rejected.
    maxFutureTime=0
    # Packets with start times this many seconds before now will be rejected.
    maxPastTime=1200
    # Heartbeats will be written approximatly this many seconds
    heartbeatInterval=30
    # Each channel has a circular buffer that attemps to hold this duration 
    # of (trace header) data in seconds.  Note, the heavy data isn't saved
    # in memory so as to keep our memory footprint low.  This number should be
    # larger than the maxPastTime.  That way, once each trace header's
    # cache is full, very old data doesn't stand a chance at being
    # evaluated. 
    circularBufferDuration=3600
    # Approximately, this many seconds the log file will contain a list of
    # channels that were excluded because of duplication, future data,
    # or expired data.
    logBadDataInterval=3600
    # The directory to which the log files will be written
    logDirectory=/home/rt/ew/logs
    # Verbosity
    # 0 -> only (critical) error messages
    # 1 -> warnings and (critical) error messages
    # 2 -> information, warnings, and (critical) error messages
    # 3 -> debug, information, warnings, and (critical) error messages
    verbosity=2

   
