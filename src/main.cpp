#include <iostream>
#include <chrono>
#include <set>
#include <map>
#include <cmath>
#include <string>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/circular_buffer.hpp>
#include <deduplicator/waveRing.hpp>
#include <deduplicator/traceBuf2.hpp>
#include "version.hpp"

struct ProgramOptions
{
    void parseCommandLineOptions(int argc, char *argv[])
    {
        std::string iniFile;
        boost::program_options::options_description desc(
R"""(
The deduplicator reads TraceBuf2 data from an Earthworm ring and attempts 
to only pass-on sanitized data by:
  1. Removing future data.
  2. Removing very old data.
  3. Removing duplicate data.
The sanitized data is then dumped onto a ring.
    deduplicator --ini=deduplicator.ini
Allowed options)""");
        desc.add_options()
            ("help", "Produces this help message")
            ("ini",  boost::program_options::value<std::string> (),
                     "Defines the initialization file for this executable")
            ("version", "Displays the version number");
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            runProgram = false;
            return;
        }
        if (vm.count("version"))
        {
            std::cout << Deduplicator::Version::getVersion() << std::endl;
            runProgram = false;
            return;
        }
        if (vm.count("ini"))
        {
            iniFile = vm["ini"].as<std::string>();
            if (!std::filesystem::exists(iniFile))
            {
                throw std::runtime_error("Initialization file: " + iniFile
                                       + " does not exist");
            }
            parseInitializationFile(iniFile);
        }
        else 
        {    
            throw std::runtime_error("Initialization file was not set");
        }    
    }
    void parseInitializationFile(const std::filesystem::path &iniFile)
    {
        boost::property_tree::ptree propertyTree;
        boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

        moduleName
            = propertyTree.get<std::string> ("moduleIdentifier", moduleName);
        if (moduleName.empty())
        {
            throw std::invalid_argument("moduleIdentifier not specified");
        }
        inputRingName
            = propertyTree.get<std::string> ("inputRingName");
        if (inputRingName.empty())
        {
            throw std::invalid_argument("inputRingName not specified");
        } 
        outputRingName
            = propertyTree.get<std::string> ("outputRingName");
        if (outputRingName.empty())
        {
            throw std::invalid_argument("outputRingName not specified");
        }
        auto logDirectoryName
            = propertyTree.get<std::string> ("logDirectory", logDirectory);
        if (logDirectoryName.empty()){logDirectoryName = "./";}
        logDirectory = logDirectoryName;
        if (!std::filesystem::exists(logDirectory))
        {
            std::filesystem::create_directories(logDirectory);
        }
        if (!std::filesystem::exists(logDirectory))
        {
            throw std::runtime_error("Could not create log directory: "
                                   + logDirectory.string());
        }

        int time
            = propertyTree.get<int> ("maxFutureTime",
                                     static_cast<int> (maxFutureTime.count()));
        maxFutureTime = std::chrono::seconds {time};
        if (maxFutureTime < std::chrono::seconds {0})
        {
            throw std::invalid_argument("Max future time is negative");
        }

        time
            = propertyTree.get<int> ("maxPastTime",
                                     static_cast<int> (maxPastTime.count()));
        maxPastTime = std::chrono::seconds {time};
        if (maxPastTime < std::chrono::seconds {0})
        {
            throw std::invalid_argument("Max past time is negative");
        } 

        time
            = propertyTree.get<int> ("heartbeatInterval",
                                  static_cast<int> (heartbeatInterval.count()));
        heartbeatInterval = std::chrono::seconds {time};
        if (heartbeatInterval < std::chrono::seconds {0})
        {
            throw std::invalid_argument("Heartbeat interval is negative");
        }

        time
            = propertyTree.get<int> ("logBadDataInterval",
                                 static_cast<int> (logBadDataInterval.count()));
        logBadDataInterval = std::chrono::seconds {time};

        time
            = propertyTree.get<int> ("circularBufferDuration",
                                 static_cast<int> (circularBufferDuration.count()));
        circularBufferDuration = std::chrono::seconds {time};
        if (circularBufferDuration < std::chrono::seconds {0})
        {
            throw std::invalid_argument("Circular buffer duration is negative");
        }

        verbosity = propertyTree.get<int> ("verbosity", verbosity);
        verbosity = std::min(3, std::max(0, verbosity));
 
    }
    std::string moduleName{"MOD_DEDUPLICATOR"};
    std::string inputRingName{"TEMP_RING"};
    std::string outputRingName{"WAVE_RING"};
    std::filesystem::path logDirectory{"./logs"};
    std::chrono::seconds maxFutureTime{0};
    std::chrono::seconds maxPastTime{1200};
    std::chrono::seconds logBadDataInterval{3600};
    std::chrono::seconds circularBufferDuration{3600};
    std::chrono::seconds heartbeatInterval{15};
    int verbosity{2};
    bool runProgram{true};
};

struct TraceHeader
{
    TraceHeader() = default;
    explicit TraceHeader(const Deduplicator::TraceBuf2 &traceBuf2) 
    {
        auto network = traceBuf2.getNetwork();
        auto station = traceBuf2.getStation(); 
        auto channel = traceBuf2.getChannel();
        std::string locationCode;
        try
        {
            locationCode = traceBuf2.getLocationCode();
        }
        catch (...)
        {
        }
        // Trace name
        name = network + "."  + station + "." + channel;
        if (!locationCode.empty())
        {
            name = name + "." + locationCode;
        }
        auto iStartTime
            = static_cast<int64_t>
              (std::round(traceBuf2.getStartTime()*1000000));
        startTime = std::chrono::microseconds {iStartTime}; 
        samplingRate
            = static_cast<int> (std::round(traceBuf2.getSamplingRate()));
        try
        {
            nSamples = traceBuf2.getNumberOfSamples();
        }
        catch (...)
        {
        }
    }
    bool operator<(const TraceHeader &rhs) const
    {
        return startTime < rhs.startTime;
    } 
    bool operator>(const TraceHeader &rhs) const
    {
        return startTime > rhs.startTime;
    }
    bool operator==(const TraceHeader &rhs) const
    {
        if (rhs.name != name){return false;}
        if (rhs.samplingRate - samplingRate != 0)
        {
            spdlog::get("deduplicator")->warn("Inconsistent samplign rates for: "
                                             + name);
            return false;
        }
        auto dStartTime = (rhs.startTime.count() - startTime.count());
        if (samplingRate < 105)
        {
            return (dStartTime < std::chrono::microseconds {15000}.count());
        }
        else if (samplingRate < 255)
        {
            return (dStartTime < std::chrono::microseconds {4500}.count());
        }
        else if (samplingRate < 505)
        {
            return (dStartTime < std::chrono::microseconds {2500}.count());
        }
        else if (samplingRate < 1005)
        {
            return (dStartTime < std::chrono::microseconds {1500}.count());
        }
        spdlog::get("deduplicator")->critical(
            "Could not classify sampling rate: "
          + std::to_string(samplingRate));
        return false;
    } 
    std::string name;
    std::chrono::microseconds startTime{0};
    int samplingRate{100};
    int nSamples{0};
};

int estimateCapacity(const TraceHeader &header,
                     const std::chrono::seconds &memory)
{
    auto duration
        = std::max(0.0,
                   std::round( (header.nSamples - 1.)
                               /std::max(1, header.samplingRate)));
    std::chrono::seconds packetDuration{static_cast<int> (duration)};
    return std::max(1000, static_cast<int> (memory.count()/duration)) + 1;
}

bool operator<(const TraceHeader &lhs, const TraceHeader &rhs)
{
    return lhs.startTime < rhs.startTime;
}

std::string toName(const Deduplicator::TraceBuf2 &traceBuf2Message)
{
    auto traceName = traceBuf2Message.getNetwork() + "." 
                   + traceBuf2Message.getStation() + "." 
                   + traceBuf2Message.getChannel();
    auto locationCode = traceBuf2Message.getLocationCode();
    if (!locationCode.empty())
    {
         traceName = traceName + "." + locationCode;
    }
    return traceName;
}

int main(int argc, char *argv[])
{
    ProgramOptions options;
    try
    {
        options.parseCommandLineOptions(argc, argv); 
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    if (!options.runProgram){return EXIT_SUCCESS;}
//return EXIT_SUCCESS;
    // Setup logger
    auto logger
        = spdlog::daily_logger_mt("deduplicator",
                                  options.logDirectory.string() + "/" + "deduplicator.log",
                                  0, 0);
    logger->set_level(spdlog::level::err);
    if (options.verbosity > 2)
    {
        logger->set_level(spdlog::level::debug);
    }
    else if (options.verbosity == 2)
    {
        logger->set_level(spdlog::level::info);
    }
    else if (options.verbosity == 1)
    {
        logger->set_level(spdlog::level::warn);
    }
    logger->info("Version: " + Deduplicator::Version::getVersion());
    logger->info("Module Identifier: " + options.moduleName);
    logger->info("Input ring: " + options.inputRingName);
    logger->info("Output ring: " + options.outputRingName);
    logger->info("Log directory: " + options.logDirectory.string());
    logger->info("Maximum future time: "
               + std::to_string(options.maxFutureTime.count()) + " seconds");
    logger->info("Maximum past time: "
               + std::to_string(options.maxPastTime.count()) + " seconds");
    logger->info("Log bad data interval: "
               + std::to_string(options.logBadDataInterval.count())
               + " seconds");
    logger->info("Approximate circular buffer duration: " 
               + std::to_string(options.circularBufferDuration.count())
               + " seconds");
    logger->info("Approximate heartbeat interval: "
               + std::to_string(options.heartbeatInterval.count()) + " seconds");


    // Create the rings
    Deduplicator::WaveRing inputWaveRing;
    try
    {
        inputWaveRing.connect(options.inputRingName);
        inputWaveRing.flush();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        logger->critical(e.what());
        return EXIT_FAILURE;
    }
    Deduplicator::WaveRing outputWaveRing; 
    try
    {
        outputWaveRing.connect(options.outputRingName,
                               options.moduleName);
        outputWaveRing.flush();
        outputWaveRing.writeHeartbeat(false);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        logger->critical(e.what());
        return EXIT_FAILURE;
    }
    auto heartbeatStartTime = std::chrono::high_resolution_clock::now();
    auto logBadDataStartTime = std::chrono::high_resolution_clock::now();
    std::set<std::string> expiredChannels;  
    std::set<std::string> futureChannels;
    std::set<std::string> duplicateChannels;
    std::map<std::string, boost::circular_buffer<::TraceHeader>> circularBuffers;
    while (true) //for (int i = 0; i < 1000; ++i)
    {
        // Begin by scraping everything off the ring
        logger->debug("Scraping ring...");
        try
        {
            inputWaveRing.read();
        }
        catch (const Deduplicator::TerminateException &e)
        {
            logger->info("Received terminate exception from ring: " 
                       + std::string {e.what()});
            break;
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            continue;
        }
        // Computing the current time after the scraping the ring is
        // conservative.  Basically, this allows for a zero-latency,
        // 1 sample packet, to be successfully passed through.
        auto now = std::chrono::high_resolution_clock::now();
        auto processingStartTime = now;
        auto nowMuS
            = std::chrono::time_point_cast<std::chrono::microseconds>
              (now).time_since_epoch().count();
        double nowSeconds = nowMuS*1.e-6;
        double earliestTime = nowSeconds - options.maxPastTime.count();
        double latestTime   = nowSeconds + options.maxFutureTime.count();
        // Unpack ring
        const auto &traceBuf2Messages
            = inputWaveRing.getTraceBuf2MessagesReference(); 
        for (const auto &traceBuf2Message : traceBuf2Messages)
        {
            // Construct the trace header for the circular buffer
            TraceHeader traceHeader;
            try
            {
                traceHeader = std::move(TraceHeader{traceBuf2Message});
            }
            catch (const std::exception &e)
            {
                logger->error("Failed to unpack traceBuf2.  Skipping...");
                continue;
            }
            
            auto startTime = traceBuf2Message.getStartTime();
            if (startTime < earliestTime)
            {
                logger->debug(traceHeader.name
                          + "'s data has expired; skipping...");
                if (!expiredChannels.contains(traceHeader.name))
                {
                    expiredChannels.insert(traceHeader.name);
                }
                continue;
            }
            auto endTime = traceBuf2Message.getEndTime();
            if (endTime > latestTime)
            {
                logger->debug(traceHeader.name
                          + "'s data is in future data; skipping...");
                if (!futureChannels.contains(traceHeader.name))
                {
                    futureChannels.insert(traceHeader.name);
                }
                continue;
            }
            // Check for existance?
            auto circularBufferIndex = circularBuffers.find(traceHeader.name);
            bool firstExample{false};
            if (circularBufferIndex == circularBuffers.end())
            {
                auto capacity
                     = estimateCapacity(traceHeader,
                                        options.circularBufferDuration);
                logger->info("Creating new circular buffer for: "
                           + traceHeader.name + " with capacity: "
                           + std::to_string(capacity));
                boost::circular_buffer<::TraceHeader> newCircularBuffer(capacity);
                newCircularBuffer.push_back(traceHeader);
                circularBuffers.insert(std::pair{traceHeader.name,
                                                 std::move(newCircularBuffer)});
                firstExample = true;
            }
            circularBufferIndex = circularBuffers.find(traceHeader.name);
            if (circularBufferIndex == circularBuffers.end())
            {
                logger->critical("Algorithm error - cb doesn't exist for: "
                               + traceHeader.name);
                continue;
            }
            auto traceHeaderIndex
                = std::find(circularBufferIndex->second.begin(),
                            circularBufferIndex->second.end(),
                            traceHeader);
            if (traceHeaderIndex != circularBufferIndex->second.end())
            {
                if (!firstExample)
                {
                    logger->debug("Detected duplicate for: "
                                + traceHeader.name);
                    if (!duplicateChannels.contains(traceHeader.name))
                    {
                        duplicateChannels.insert(traceHeader.name);
                    }
                    continue;
                }
                else
                {
                    logger->debug("Initial duplicate found for: "
                                + traceHeader.name + "; everything is fine!");
                }
            }
            // Insert it (typically new stuff shows up)
            if (traceHeader > circularBufferIndex->second.back())
            {
                logger->debug("Inserting " + traceHeader.name
                            + " at end of cb");
                circularBufferIndex->second.push_back(traceHeader);
            }
            else // This is slow but we'll do it
            {
                logger->debug("Inserting " + traceHeader.name
                            + " in cb then sorting...");
                circularBufferIndex->second.push_back(traceHeader);
                std::sort(circularBufferIndex->second.begin(),
                          circularBufferIndex->second.end());
            }
            // Write it back out
            try
            {
                outputWaveRing.write(traceBuf2Message);
            }
            catch (const std::exception &e)
            {
                logger->warn("Failed to write " + traceHeader.name
                           + " to output ring.  Failed with: "
                           + std::string{e.what()});
                continue;
            }
        } // Loop on traces
        // Time for heartbeating
        auto heartbeatDuration
            = std::chrono::duration_cast<std::chrono::seconds>
              (now - heartbeatStartTime);
        if (heartbeatDuration > options.heartbeatInterval)
        {
            try
            {
                outputWaveRing.writeHeartbeat(false);
            }
            catch (const std::exception &e)
            {
                logger->error(e.what());
            }
            heartbeatStartTime = now; 
        }
        // Time for logging?
        auto logBadDataDuration
            = std::chrono::duration_cast<std::chrono::seconds>
              (now - logBadDataStartTime);
        if (logBadDataDuration > options.logBadDataInterval &&
            options.logBadDataInterval.count() >= 0)
        {
            if (!expiredChannels.empty())
            {
                std::string message{"The following channels had expired data:"};
                for (const auto &expiredChannel : expiredChannels)
                {
                    message = message + " " + expiredChannel;
                }
                logger->info(message);
            }
            if (!futureChannels.empty())
            {
                std::string message{"The following channels had future data:"};
                for (const auto &futureChannel : futureChannels)
                {
                    message = message + " " + futureChannel;
                }
                logger->info(message);
            } 
            if (!duplicateChannels.empty())
            {
                std::string message{"The following channels had duplicate data:"};
                for (const auto &duplicateChannel : duplicateChannels)
                {
                    message = message + " " + duplicateChannel;
                }
                logger->info(message);
            }
            // Reset for next interval
            logBadDataStartTime = now;
            expiredChannels.clear();
            futureChannels.clear();
            duplicateChannels.clear();
        }
        // Don't want to slam the ring but also don't want to slow ourselves
        // down too much under a heavy load.
        auto processingEndTime = std::chrono::high_resolution_clock::now(); 
        auto processingDuration
            = std::chrono::duration_cast<std::chrono::milliseconds>
              (processingStartTime - processingEndTime);
        constexpr std::chrono::milliseconds oneSecond{1000};
        if (processingDuration < oneSecond)
        { 
            std::this_thread::sleep_for(oneSecond - processingDuration);
        }
    }
    outputWaveRing.writeHeartbeat(true);
    return EXIT_SUCCESS;
}
