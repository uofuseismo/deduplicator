#define WITH_EARTHWORM
#define _UNIX
#include <iostream>
#include <chrono>
#include <array>
#include <cstring>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>
#undef WITH_MSEED
#ifdef WITH_MSEED
#include <libmseed.h>
#endif
#ifdef WITH_EARTHWORM
extern "C"
{
#include <transport.h>
#include <earthworm_simple_funcs.h>
#include <trace_buf.h>
}
#endif
#include <deduplicator/waveRing.hpp>
#include <deduplicator/traceBuf2.hpp>

using namespace Deduplicator;

namespace
{
/// This is klunky but effectively earthworm doesn't use const char * which
/// drives C++ nuts.  So we require some fixed size containers to hold 
/// earthworm types.
#ifdef WITH_MSEED
std::array<char, 12> TYPE_MSEED{"TYPE_MSEED\0"};
#endif
std::array<char, 12> TYPE_ERROR{"TYPE_ERROR\0"};
std::array<char, 14> MOD_WILDCARD{"MOD_WILDCARD\0"};
std::array<char, 15> INST_WILDCARD{"INST_WILDCARD\0"};
std::array<char, 16> TYPE_HEARTBEAT{"TYPE_HEARTBEAT\0"};
std::array<char, 16> TYPE_TRACEBUF2{"TYPE_TRACEBUF2\0"};
//std::array<char, 21> TYPE_TRACECOMP2{"TYPE_TRACE2_COMP_UA\0"};
}

class WaveRing::WaveRingImpl
{
public:
    /// Earthworm messages
    std::vector<TraceBuf2> mTraceBuf2Messages;
    /// Logos to scrounge from the ring.
    std::vector<MSG_LOGO> mLogos;
    std::string mRingName;
    /// Earthworm shared memory region corresponding to the
    /// earhtworm ring.
    SHM_INFO mRegion;
    long mRingKey{0};
    uint32_t mMilliSecondsWait{0};
    /// Earthworm installation ID
    unsigned char mInstallationIdentifier{0};
    /// Installation wildcard
    unsigned char mInstallationWildCard{0};
    /// Module identifier
    unsigned char mModuleIdentifier{0};
    /// Heartbeat type
    unsigned char mHeartBeatType{0};
    /// Tracebuffer2 type
    unsigned char mTraceBuffer2Type{0};
    /// TraceComp2
    //unsigned char mTraceComp2Type{0};
#ifdef WITH_MSEED
    /// MSEED type
    unsigned char mMSEEDType{0};
#endif
    /// Module wildcard
    unsigned char mModWildCard{0};
    /// Error type
    unsigned char mErrorType{0};
    /// Most waves read off the ring
    int mMostWavesRead{0};
    /// Process identifier
    int mProcessIdentifier{getpid()};
    /// Have the region?
    bool mHaveRegion{false};
    /// Connected?
    bool mConnected{false};
};

/// C'tor
WaveRing::WaveRing() :
    pImpl(std::make_unique<WaveRingImpl> ())
{
}

/// Move assignment
WaveRing::WaveRing(WaveRing &&waveRing) noexcept
{
    *this = std::move(waveRing);
}

/// Move assignment
WaveRing& WaveRing::operator=(WaveRing &&waveRing) noexcept
{
    if (&waveRing == this){return *this;}
    pImpl = std::move(waveRing.pImpl); 
    return *this;
}

/// Destructor
WaveRing::~WaveRing()
{
    disconnect();
}

/// Disconnects
void WaveRing::disconnect() noexcept
{
#ifdef WITH_EARTHWORM
    if (pImpl->mHaveRegion)
    {
        spdlog::get("deduplicator")->info("Disconnecting from ring...");
        tport_detach(&pImpl->mRegion);
    }
    memset(&pImpl->mRegion, 0, sizeof(SHM_INFO));
    pImpl->mTraceBuf2Messages.clear();
    pImpl->mLogos.clear();
    pImpl->mRingName.clear();
    pImpl->mRingKey = 0;
    pImpl->mMilliSecondsWait = 0;
    pImpl->mInstallationIdentifier = 0;
    pImpl->mInstallationWildCard = 0;
    pImpl->mHeartBeatType = 0;
    pImpl->mTraceBuffer2Type = 0;
    pImpl->mModuleIdentifier = 0;
    //pImpl->mTraceComp2Type = 0;
#ifdef WITH_MSEED
    pImpl->mMSEEDType = 0;
#endif
    pImpl->mModWildCard = 0;
    pImpl->mErrorType = 0;
    pImpl->mMostWavesRead = 0;
    pImpl->mProcessIdentifier = getpid();
    pImpl->mHaveRegion = false;
    pImpl->mConnected = false;
#endif
}

/// Connect
void WaveRing::connect(const std::string &ringName,
                       const std::string &moduleName)
{
    // Checks
    if (!haveEarthworm()){throw std::runtime_error("Recompile with earthworm");}
    if (ringName.empty()){throw std::invalid_argument("ringName is empty");}
    // Make sure I'm not already connected
    disconnect();
#ifdef WITH_EARTHWORM
    // Get the ring key.  Note, earthworm doesn't believe in const so
    // this is a workaround 
    spdlog::get("deduplicator")->debug("Getting key from ring: " + ringName);
    std::vector<char> ringNameWork(ringName.size() + 1, '\0');
    std::copy(ringName.begin(), ringName.end(), ringNameWork.begin());
    pImpl->mRingKey = GetKey(ringNameWork.data());
    // Attach to the ring
    spdlog::get("deduplicator")->debug("Attaching to ring: " + ringName);
    tport_attach(&pImpl->mRegion, pImpl->mRingKey);
    pImpl->mHaveRegion = true;
    if (pImpl->mRingKey ==-1)
    {
        spdlog::get("deduplicator")->error("Failed to get key for ring: "
                                         + ringName);
        return;
    }
    // Installation information
    spdlog::get("deduplicator")->debug("Specifying logos...");
    if (GetLocalInst(&pImpl->mInstallationIdentifier) != 0)
    {
        throw std::runtime_error("Failed to get installation identifier");
    }
    // Various types
    if (GetType(TYPE_TRACEBUF2.data(), &pImpl->mTraceBuffer2Type) != 0)
    {
        throw std::runtime_error("Failed to get tracebuf2 type");
    }
#ifdef WITH_MSEED
    if (GetType(TYPE_MSEED.data(), &pImpl->mMSEEDType) != 0)
    {
        throw std::runtime_error("Failed to get MSEED type");
    }
#endif
    if (GetType(TYPE_HEARTBEAT.data(), &pImpl->mHeartBeatType) != 0)
    {
        throw std::runtime_error("Failed to get heartbeat type");
    }
    if (GetType(TYPE_ERROR.data(), &pImpl->mErrorType) != 0)
    {
        throw std::runtime_error("Failed to get error type");
    }
    // Wildcard info
    if (GetInst(INST_WILDCARD.data(), &pImpl->mInstallationWildCard) != 0)
    {
        throw std::runtime_error("Failed to get installation wildcard");
    }
    if (GetModId(MOD_WILDCARD.data(), &pImpl->mModWildCard) != 0)
    {
        throw std::runtime_error("Failed to get wildcard module ID");
    }
    if (!moduleName.empty())
    {
        auto work = moduleName;
        if (GetModId(work.data(), &pImpl->mModuleIdentifier) != 0)
        {
            throw std::runtime_error("Failed to get module identifier");
        }
        spdlog::get("deduplicator")->info("Got module ID: "
             + std::to_string (static_cast<int> (pImpl->mModuleIdentifier)));
    }
    else
    {
        pImpl->mModuleIdentifier = pImpl->mModWildCard;
    }
    // Create the logos we wish to read
    pImpl->mLogos.clear();
    MSG_LOGO traceBuf2Logo;
    memset(&traceBuf2Logo, 0, sizeof(MSG_LOGO));
    traceBuf2Logo.type = pImpl->mTraceBuffer2Type;
    pImpl->mLogos.push_back(traceBuf2Logo);

/*
    MSG_LOGO traceComp2Logo;
    memset(&traceComp2Logo, 0, sizeof(MSG_LOGO));
    traceComp2Logo.type = pImpl->mTraceComp2Type;
    pImpl->mLogos.push_back(traceComp2Logo);
*/

#ifdef WITH_MSEED
    MSG_LOGO mseedLogo;
    memset(&mseedLogo, 0, sizeof(MSG_LOGO));
    mseedLogo.type = pImpl->mMSEEDType;
    pImpl->mLogos.push_back(mseedLogo);
#endif
    // Copy some stuff now that we have survived
    pImpl->mRingName = ringName;
    pImpl->mMilliSecondsWait = 0;
    pImpl->mProcessIdentifier = getpid();
    pImpl->mConnected = true;
    // Optimization -> reserve some space
    pImpl->mTraceBuf2Messages.reserve(1024);
    spdlog::get("deduplicator")->info("Connect to " + ringName + "!");
#endif
}

/// Connected?
bool WaveRing::isConnected() const noexcept
{
    return pImpl->mConnected;
}

/// Writes a heartbeat message to the ring
void WaveRing::writeHeartbeat(const bool terminate)
{
    if (!haveEarthworm()){throw std::runtime_error("Recompile with earthworm");}
    if (!isConnected()){throw std::runtime_error("Not connected to a ring");}
    auto processIdentifier = static_cast<int64_t> (pImpl->mProcessIdentifier);
    auto nowSeconds
        = std::chrono::time_point_cast<std::chrono::seconds>
              (std::chrono::high_resolution_clock::now())
              .time_since_epoch().count();
    std::string message;
    if (!terminate)
    {
        message = std::to_string(nowSeconds)
                + " "
                + std::to_string(processIdentifier)
                + "\n";
    }
    else
    {
        message = std::to_string(nowSeconds) + " -1 Terminating!\n";
    }
    MSG_LOGO logo;
    std::memset(&logo, 0, sizeof(MSG_LOGO));
    logo.instid = pImpl->mInstallationIdentifier;
    logo.mod = pImpl->mModuleIdentifier;
    logo.type = pImpl->mHeartBeatType;
    spdlog::get("deduplicator")->debug("Writing status message: " + message);
    auto result = tport_putmsg(&pImpl->mRegion, &logo,
                               message.size(), message.data());
    if (result != PUT_OK)
    {   
        throw std::runtime_error("Failed to write heartbeat to ring");
    }
}

/// Writes message to the ring
void WaveRing::write(const TraceBuf2 &message)
{
    if (!haveEarthworm()){throw std::runtime_error("Recompile with earthworm");}
    if (!isConnected()){throw std::runtime_error("Not to connected to a ring");}
    MSG_LOGO logo;
    std::memset(&logo, 0, sizeof(MSG_LOGO));
    logo.instid = pImpl->mInstallationIdentifier; //mInstallationWildCard;
    logo.mod = pImpl->mModuleIdentifier;
    logo.type = pImpl->mTraceBuffer2Type;
    std::array<char, MAX_TRACEBUF_SIZ> output;
    auto messageLength = message.getMessageLength();
    auto messagePtr = message.getNativePacketPointer();
    std::copy(messagePtr, messagePtr + messageLength, output.begin());
    std::fill(output.begin() + messageLength, 
              output.end(),
              '\0');
    auto result = tport_putmsg(&pImpl->mRegion, &logo,
                               messageLength, output.data());
    if (result != PUT_OK)
    {
        auto name = message.getNetwork() + "." 
                  + message.getStation() + "."
                  + message.getChannel();
        auto location = message.getLocationCode();
        if (!location.empty()){name = name + "." + location;}
        throw std::runtime_error("Failed to put " + name + " onto ring");
    }
}

/// Reads message from the ring
void WaveRing::read()
{
    if (!haveEarthworm()){throw std::runtime_error("Recompile with earthworm");}
#ifdef WITH_EARTHWORM
    if (!isConnected()){throw std::runtime_error("Not to connected to a ring");}
    // The algorithm works as follows:
    //  (1) Take the information off the ring as fast as possible.
    //  (2) Unpack the tracebuffers
    // To do (1) first attempt to allocate enough space. 
    int nWork = std::max(1024, pImpl->mMostWavesRead);
    std::vector<std::array<char, MAX_TRACEBUF_SIZ>> messageWork;
    std::vector<unsigned char> messageType;
    std::vector<long> messageLength;
    messageWork.reserve(nWork);
    messageType.reserve(nWork);
    messageLength.reserve(nWork);
    pImpl->mTraceBuf2Messages.resize(0);
    // Now copy the (unpacked) messages from the ring
    std::array<char, MAX_TRACEBUF_SIZ> msg;
    MSG_LOGO gotLogo;
    long gotSize = 0;
    int returnCode = 0;
    unsigned char sequenceNumber;
    int nRead = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while(true)
    {
        // Not really sure what to do with a kill signal
        returnCode = tport_getflag(&pImpl->mRegion);
        if (returnCode == TERMINATE)
        {
            auto error = "Receiving kill signal from ring: " + pImpl->mRingName
                       + "\nDisconnecting from ring...";
            spdlog::get("deduplicator")->error(error);
            disconnect();
            throw TerminateException(error);//std::runtime_error(error);
        }
        // Copy the ring message
        std::fill(msg.begin(), msg.end(), '\0');
        returnCode = tport_copyfrom(&pImpl->mRegion,
                                    pImpl->mLogos.data(),
                                    pImpl->mLogos.size(),
                                    &gotLogo, &gotSize,
                                    msg.data(), MAX_TRACEBUF_SIZ,
                                    &sequenceNumber);
        // Are we done?
        if (returnCode == GET_NONE){break;}
        // Handle earthworm errors
        if (returnCode != GET_OK)
        {
            if (returnCode == GET_MISS)
            {
                spdlog::get("deduplicator")->warn("Some messages were missed");
            }
            else if (returnCode == GET_NOTRACK)
            {
                spdlog::get("deduplicator")->warn(
                    "Message exceeded NTRACK_GET");
            }
            else if (returnCode == GET_TOOBIG)
            {
                spdlog::get("deduplicator")->warn("TraceBuf2 message too big");
            }
            else if (returnCode == GET_MISS_LAPPED)
            {
                spdlog::get("deduplicator")->warn(
                    "Some messages were overwritten");
            }
            else if (returnCode == GET_MISS_SEQGAP)
            {
                spdlog::get("deduplicator")->warn(
                    "A gap in messages was detected");
            }
            else
            {
                spdlog::get("deduplicator")->warn(
                    "Unknown earthworm error: "
                  +  std::to_string(returnCode));
            }
            continue;
        }
        // Unpack the tracebuf2 type message
        if (gotLogo.type == pImpl->mTraceBuffer2Type)
        {
            // Note, there's an optimization to be had by only copying 
            // gotSize bytes.  But for now, this is simple in terms of
            // memory (re)allocation.
            messageWork.push_back(msg);
            messageType.push_back(gotLogo.type);
            messageLength.push_back(gotSize);
        }
#ifdef WITH_MSEED
        else if (gotLogo.type == pImpl->mMSEEDType)
        {
            spdlog::get("deduplicator")->error("MSEED message not handled");
            messageWork.push_back(msg);
            messageType.push_back(gotLogo.type);
            messageLength.push_back(gotSize);
        }
#endif
        else
        {
            spdlog::get("deduplicator")->error("Unhandled message type");
            continue;
        }
        nRead = nRead + 1;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration<double> (end - start).count();
    if (pImpl->mMilliSecondsWait > 0){sleep_ew(pImpl->mMilliSecondsWait);}
    // Update our typical allocation size
    pImpl->mMostWavesRead = std::max(pImpl->mMostWavesRead, 
                                     static_cast<int> (messageWork.size()));
    // Step 2: Unpack the messages as fast as possible
    auto nTraceBuf2Messages = std::count(messageType.begin(),
                                         messageType.end(),
                                         pImpl->mTraceBuffer2Type);
    if (nTraceBuf2Messages > 0)
    {
        start = std::chrono::high_resolution_clock::now();
        pImpl->mTraceBuf2Messages.resize(messageWork.size());
        for (int it = 0; it < static_cast<int> (messageWork.size()); ++it)
        {
            if (messageType[it] == pImpl->mTraceBuffer2Type)
            {
                try
                {
                    pImpl->mTraceBuf2Messages[it].fromEarthworm(
                        messageWork[it].data(),
                        messageLength[it]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << e.what() << std::endl;
                    continue;
                }
            }
        }
        // Evict any empty messages
        pImpl->mTraceBuf2Messages.erase(
            std::remove_if(pImpl->mTraceBuf2Messages.begin(),
                           pImpl->mTraceBuf2Messages.end(),
                           [](const TraceBuf2 &tb2)
                           {
                              return tb2.getNumberOfSamples() == 0;
                           }),
                           pImpl->mTraceBuf2Messages.end());
        end = std::chrono::high_resolution_clock::now();
        elapsedTime = std::chrono::duration<double> (end - start).count();
    }
#ifdef WITH_MSEED
    auto nMSEEDMessages = std::count(messageType.begin(),
                                     messageType.end(),
                                     pImpl->mMSEEDType);
    if (nMSEEDMessages > 0)
    {
        std::cerr << 
           "Need loop to unpack MSEED messages with msr_unpack" << std::endl;
        for (int it = 0; it < static_cast<int> (messageWork.size()); ++it)
        {
            if (messageType[it] == pImpl->mMSEEDType)
            {
                MS3Record *msr = nullptr;
                auto size = messageWork[it].size();
                msr3_parse(messageWork[it].data(), size, &msr, 1, 0);
                msr3_free(&msr);
            }
        }
    }
#endif
#endif // End on earthworm
}

/// Flushes the wave ring
void WaveRing::flush()
{
    if (!haveEarthworm()){throw std::runtime_error("Recompile with earthworm");}
#ifdef WITH_EARTHWORM
    if (!isConnected()){throw std::runtime_error("Not to connected to a ring");}
    spdlog::get("deduplicator")->debug("Flushing ring...");
    MSG_LOGO gotLogo;
    std::array<char, MAX_TRACEBUF_SIZ> msg;
    long gotSize = 0;
    int returnCode = 0;
    unsigned char sequenceNumber;
    int nMessages = 1;
    while (true)
    {
        returnCode = tport_copyfrom(&pImpl->mRegion,
                                    pImpl->mLogos.data(),
                                    pImpl->mLogos.size(),
                                    &gotLogo, &gotSize,
                                    msg.data(), MAX_TRACEBUF_SIZ,
                                    &sequenceNumber);
        if (returnCode == GET_NONE){break;}
        nMessages = nMessages + 1;
    }
    spdlog::get("deduplicator")->debug("Flushed " + std::to_string(nMessages));
    if (pImpl->mMilliSecondsWait > 0){sleep_ew(pImpl->mMilliSecondsWait);}
#endif
    pImpl->mTraceBuf2Messages.clear();
}

/// Have earthworm?
bool WaveRing::haveEarthworm() const noexcept
{
#ifdef WITH_EARTHWORM
    return true;
#else
    return false;
#endif
}

/// Get tracebuf2 messages
std::vector<TraceBuf2> 
    WaveRing::getTraceBuf2Messages() const noexcept
{
    return pImpl->mTraceBuf2Messages;
}

const TraceBuf2 *WaveRing::getTraceBuf2MessagesPointer() const noexcept
{
    return pImpl->mTraceBuf2Messages.data();
}

const std::vector<TraceBuf2>
&WaveRing::getTraceBuf2MessagesReference() const noexcept
{
    return pImpl->mTraceBuf2Messages;
}

std::vector<TraceBuf2> WaveRing::moveTraceBuf2Messages() noexcept
{
    auto result = std::move(pImpl->mTraceBuf2Messages);
    std::vector<TraceBuf2> newMessages;
    pImpl->mTraceBuf2Messages = newMessages;
    return result;
}

int WaveRing::getNumberOfTraceBuf2Messages() const noexcept
{
    return static_cast<int> (pImpl->mTraceBuf2Messages.size());
}
