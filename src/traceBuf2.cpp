#define WITH_EARTHWORM
#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <array>
#include <algorithm>
#include <string>
#include <cassert>
#include <bit>
#include <deduplicator/traceBuf2.hpp>
#ifdef WITH_EARTHWORM
   #include "trace_buf.h"
   #define MAX_TRACE_SIZE (MAX_TRACEBUF_SIZ - 64)
   #define STA_LEN (TRACE2_STA_LEN  - 1)
   #define NET_LEN (TRACE2_NET_LEN  - 1)
   #define CHA_LEN (TRACE2_CHAN_LEN - 1)
   #define LOC_LEN (TRACE2_LOC_LEN  - 1)
#else
   // These values are from Earthworm's trace_buf.h but subtracted by 1
   // since std::string will handle the NULL termination for us.
   #define MAX_TRACE_SIZE 4032 //4096 - 64 
   #define STA_LEN 6
   #define NET_LEN 8
   #define CHA_LEN 3
   #define LOC_LEN 2
#endif

using namespace Deduplicator;
 
namespace
{
/// Copies an input network/station/channel/location code while respecting
/// the max size for the parameter.
void copyString(std::string *result,
                const std::string &sIn, const int outputLength)
{
    auto nCopy = std::min(sIn.size(), static_cast<size_t> (outputLength));
    result->resize(nCopy);
    std::copy(sIn.c_str(), sIn.c_str() + nCopy, result->begin());
}

/// Utility function to define the appropriate format
template<typename T> 
std::string getDataFormat() noexcept
{
    std::string result(2, '\0'); 
    if constexpr (std::endian::native == std::endian::little) 
    {
        if constexpr (std::is_same<T, double>::value)
        {
            result = "f8";
        }
        else if constexpr (std::is_same<T, float>::value)
        {
            result = "f4";
        }
        else if constexpr (std::is_same<T, int>::value)
        {
            result = "i4";
        }
        else if constexpr (std::is_same<T, int16_t>::value)
        {
            result = "i2";
        }
        else
        {
            spdlog::get("deduplicator")->critical(
                "Unhandled little endian precision");
#ifndef NDEBUG
            assert(false);
#endif
        }
    }
    else if constexpr (std::endian::native == std::endian::big)
    {
        if constexpr (std::is_same<T, double>::value)
        {   
            result = "t8";
        }   
        else if constexpr (std::is_same<T, float>::value)
        {   
            result = "t4";
        }
        else if constexpr (std::is_same<T, int>::value)
        {   
            result = "s4";
        }   
        else if constexpr (std::is_same<T, int16_t>::value)
        {   
            result = "s2";
        }
        else
        {
            spdlog::get("deduplicator")->critical(
                "Unhandled big endian precision");
#ifndef NDEBUG
            assert(false);
#endif
        }
    }
    else
    {
        spdlog::get("deduplicator")->critical("Mixed endianness is unhandled");
#ifndef NDEBUG
        assert(false);
#endif
    } 
    return result;
}

/// Utility function to define the correct message length
template<typename T>  
int getMaxTraceLength() noexcept
{
#ifndef NDEBUG
    assert(MAX_TRACE_SIZE%sizeof(T) == 0);
#endif
    return MAX_TRACE_SIZE/sizeof(T);
}

/// Unpacks data
template<typename T> T unpack(const char *__restrict__ cIn,
                              const bool swap = false)
{
    union
    {
        char c[sizeof(T)];
        T value;
    };
    if (!swap)
    {
        std::copy(cIn, cIn + sizeof(T), c);
    }
    else
    {
        std::reverse_copy(cIn, cIn + sizeof(T), c);
    }
    return value;
}

/// Packs data
template<typename T>
void pack(const T inputValue, char *packedData, const bool swap = false)
{
    union
    {
         char c[sizeof(T)];
         T value;
    };
    value = inputValue; 
    if (!swap)
    {
        std::copy(c, c + sizeof(T), packedData);
    }
    else
    {
        std::reverse_copy(c, c + sizeof(T), packedData);
    }
}

template<typename T, typename U> std::vector<T> 
unpack(const char *__restrict__ cIn, const int nSamples, const bool swap)
{
    std::vector<T> result(nSamples);
    if (!swap)
    {
        auto dPtr = reinterpret_cast<const U *__restrict__> (cIn);
        std::copy(dPtr, dPtr + nSamples, result.data());
    }
    else
    {
        const auto nBytes = sizeof(U);
        auto resultPtr = result.data();
        for (int i = 0; i < nSamples; ++i)
        {
            resultPtr[i] = static_cast<T> (unpack<U>(cIn + i*nBytes, swap));
        }
    }
    return result;
}

TraceBuf2 unpackEarthwormMessage(const char *message, const size_t messageLength)
{
#ifndef NDEBUG
    assert(messageLength <= MAX_TRACEBUF_SIZ);
#endif
    // Bytes  0 - 3:  pinno (int)
    // Bytes  4 - 7:  nsamp (int)
    // Bytes  8 - 15: starttime (double)
    // Bytes 16 - 23: endtime (double)
    // Bytes 24 - 31: sampling rate (double)
    // Bytes 32 - 38: station (char)
    // Bytes 39 - 44: network (char)
    // Bytes 48 - 51: channel (char)
    // Bytes 52 - 54: location (char)
    // Bytes 55 - 56: version (char)
    // Bytes 57 - 59: datatype (char) 
    // Bytes 60 - 61: quality (char)
    // Bytes 62 - 63: pad (char) 
    TraceBuf2 result;
    // First figure out the data format (int, double, float, etc.)
    bool swap = false;
    char dtype = 'i';
    if (message[57] == 'i')
    {
        if constexpr (std::endian::native == std::endian::big){swap = true;}
        dtype = 'i';
    }
    else if (message[57] == 'f')
    {
        if constexpr (std::endian::native == std::endian::big){swap = true;} 
        dtype = 'f';
    }
    else if (message[57] == 's')
    {
        if constexpr (std::endian::native == std::endian::little){swap = true;}
        dtype = 'i';
    }
    else if (message[57] == 't')
    {
        if constexpr (std::endian::native == std::endian::little){swap = true;}
        dtype = 'f';
    }
    // Now figure out the number of bytes
    int nBytes = 4;
    if (message[58] == '4')
    {
        nBytes = 4;
    }
    else if (message[58] == '2')
    {
        nBytes = 2;
        if (dtype == 'f')
        {
            spdlog::get("deduplicator")->critical(
                "Unhandled float16");
#ifndef NDEBUG
            assert(false);
#endif
            return result; 
        }
    }
    else if (message[58] == '8')
    {
        nBytes = 8;
    }
    else
    {
        spdlog::get("deduplicator")->critical(
             "Unhandled number of bytes");
#ifndef NDEBUG
        assert(false);
#endif
        return result;
    }
    //auto messageLength = strnlen(message, MAX_TRACEBUF_SIZ);

    result.setNativePacket(message, messageLength); // Straight save the packet
    // Unpack some character info
    std::string station(message + 32);
    std::string network(message + 39);
    std::string channel(message + 48);
    std::string location(message + 52); 
    result.setNetwork(network);
    result.setStation(station);
    result.setChannel(channel);
    result.setLocationCode(location);
    // Finally unpack the data
    if (!swap)
    {
        constexpr bool swapPass = false;
        auto pinno        = ::unpack<int>(&message[0],      swapPass);
        auto nsamp        = ::unpack<int>(&message[4],      swapPass);
        auto startTime    = ::unpack<double>(&message[8],   swapPass);
        //auto endTime    = ::unpack<double>(&message[16],  swapPass);
        auto samplingRate = ::unpack<double>(&message[24],  swapPass);
        auto quality      = ::unpack<int16_t>(&message[60], swapPass);
        result.setPinNumber(pinno);
        result.setStartTime(startTime);
        result.setSamplingRate(samplingRate);
        result.setQuality(quality);
        result.setNumberOfSamples(nsamp);
/*

        if (dtype == 'i')
        {
            if (nBytes == 2)
            {
                auto dPtr = reinterpret_cast<const int16_t *> (message + 64);
                result.setData(nsamp, dPtr);
            }
            else if (nBytes == 4)
            {
                auto dPtr = reinterpret_cast<const int32_t *> (message + 64);
                result.setData(nsamp, dPtr);
            }
            else if (nBytes == 8)
            {
                auto dPtr = reinterpret_cast<const int64_t *> (message + 64);
                result.setData(nsamp, dPtr);
            }
        }
        else if (dtype == 'f' && nBytes == 4)
        {
            if (nBytes == 4)
            {
                auto dPtr = reinterpret_cast<const float *> (message + 64);
                result.setData(nsamp, dPtr);
            }
            else if (nBytes == 8)
            {
                auto dPtr = reinterpret_cast<const double *> (message + 64);
                result.setData(nsamp, dPtr);
            }
        }
        else
        {
           std::cerr << "Can only process i or f datatype" << std::endl;
#ifndef NDEBUG
           assert(false);
#endif
        }
*/
    }
    else
    {
        constexpr bool swapPass = true;
        auto pinno        = unpack<int>(&message[0],      swapPass);
        auto nsamp        = unpack<int>(&message[4],      swapPass);
        auto startTime    = unpack<double>(&message[8],   swapPass);
        auto samplingRate = unpack<double>(&message[24],  swapPass);
        auto quality      = unpack<int16_t>(&message[60], swapPass);
        result.setPinNumber(pinno);
        result.setStartTime(startTime);
        result.setSamplingRate(samplingRate);
        result.setQuality(quality);
        result.setNumberOfSamples(nsamp);

/*
        // Unpack the data as a debugging activity
        std::vector<T> x;
        if (dtype == 'i')
        {
            if (nBytes == 2)
            {
                x = ::unpack<T, int16_t>(message + 64, nsamp, swapPass);
            }
            else if (nBytes == 4)
            {
                x = ::unpack<T, int32_t>(message + 64, nsamp, swapPass);
            }
            else if (nBytes == 8)
            {
                x = ::unpack<T, int64_t>(message + 64, nsamp, swapPass);
            }
        }
        else if (dtype == 'f' && nBytes == 4)
        {
            if (nBytes == 4)
            {
                x = ::unpack<T, float>(message + 64, nsamp, swapPass);
            }
            else if (nBytes == 8)
            {
                x = ::unpack<T, double>(message + 64, nsamp, swapPass);
            }
        }
        else
        {
#ifndef NDEBUG
           assert(false);
#endif
        }
*/
    }
    return result; 
}

}

/// The implementation
class TraceBuf2::TraceBuf2Impl
{
public:
    void updateEndTime()
    {
        mEndTime = mStartTime;
        if (mSamples > 0 && mSamplingRate > 0)
        {
            mEndTime = mStartTime
                     + static_cast<double> (mSamples - 1)/mSamplingRate;
        }
    }
    void clear() noexcept
    {
        //mData.clear();
        mNetwork.clear();
        mStation.clear();
        mChannel.clear();
        mLocationCode.clear();
        mVersion = "20";
        mMessageLength = 0;
        mQuality = 0;//"\0\0";
        mStartTime = 0;
        mEndTime = 0;
        mSamplingRate = 0;
        mPinNumber = 0;
    }
    /// A simple copy of the data from the ring
    std::array<char, MAX_TRACEBUF_SIZ> mRawData;  
    /// The data in the packet.
    //std::vector<T> mData; 
    /// The network code.
    std::string mNetwork;//{NET_LEN, '\0'};
    /// The station code.
    std::string mStation;//{STA_LEN, '\0'};
    /// The channel code.
    std::string mChannel;//{CHA_LEN, '\0'};
    /// The location code.
    std::string mLocationCode;//{LOC_LEN, '-'};
    /// Default to version 2.0.
    std::string mVersion{"20"};
    /// Message size
    size_t mMessageLength{0};
    /// The data format
    //const std::string mDataType = getDataFormat<T>();
    /// The quality
    //std::string mQuality{2, '\0'}; // Default to no quality
    /// The UTC time of the first sample in seconds from the epoch.
    double mStartTime{0};
    /// The UTC time of the last sample in seconds from the epoch.
    double mEndTime{0};
    /// The sampling rate in Hz.
    double mSamplingRate{0};
    /// The pin number.
    int mPinNumber{0};
    /// Data quality
    int mQuality{0};
    /// Number of samples
    int mSamples{0};
    /// Max trace length
    //const int mMaximumNumberOfSamples{getMaxTraceLength<int>()};
};

/// C'tor
TraceBuf2::TraceBuf2() :
    pImpl(std::make_unique<TraceBuf2Impl> ())
{
}

/// Copy c'tor
TraceBuf2::TraceBuf2(const TraceBuf2 &traceBuf2)
{
    *this = traceBuf2;
}

/// Move c'tor
TraceBuf2::TraceBuf2(TraceBuf2 &&traceBuf2) noexcept
{
    *this = std::move(traceBuf2);
}

/// Copy assignment
TraceBuf2 &TraceBuf2::operator=(const TraceBuf2 &traceBuf2)
{
    if (&traceBuf2 == this){return *this;}
    pImpl = std::make_unique<TraceBuf2Impl> (*traceBuf2.pImpl);
    return *this;
}

/// Move assignment
TraceBuf2 &TraceBuf2::operator=(TraceBuf2 &&traceBuf2) noexcept
{
    if (&traceBuf2 == this){return *this;}
    pImpl = std::move(traceBuf2.pImpl);
    return *this;
}

/// Set the network name
void TraceBuf2::setNetwork(const std::string &network) noexcept
{
    ::copyString(&pImpl->mNetwork, network,
                 TraceBuf2::getMaximumNetworkLength());
}

std::string TraceBuf2::getNetwork() const noexcept
{
    return pImpl->mNetwork;
}

int TraceBuf2::getMaximumNetworkLength() noexcept
{
    return NET_LEN;
} 

/// Set the station name
void TraceBuf2::setStation(const std::string &station) noexcept
{
    ::copyString(&pImpl->mStation, station,
                 TraceBuf2::getMaximumStationLength());
}

std::string TraceBuf2::getStation() const noexcept
{
    return pImpl->mStation;
}

int TraceBuf2::getMaximumStationLength() noexcept
{
    return STA_LEN;
}

/// Set the channel name
void TraceBuf2::setChannel(const std::string &channel) noexcept
{
    ::copyString(&pImpl->mChannel, channel,
                 TraceBuf2::getMaximumChannelLength());
}

std::string TraceBuf2::getChannel() const noexcept
{
    return pImpl->mChannel;
}

int TraceBuf2::getMaximumChannelLength() noexcept
{
    return CHA_LEN;
}

/// Set the location code
void TraceBuf2::setLocationCode(const std::string &location) noexcept
{
    ::copyString(&pImpl->mLocationCode, location,
                 TraceBuf2::getMaximumLocationCodeLength());
}

std::string TraceBuf2::getLocationCode() const noexcept
{
    return pImpl->mLocationCode;
}

int TraceBuf2::getMaximumLocationCodeLength() noexcept
{
    return LOC_LEN;
}

/// Set the start time
void TraceBuf2::setStartTime(const double startTime) noexcept
{
    pImpl->mStartTime = startTime;
    pImpl->updateEndTime();
}

double TraceBuf2::getStartTime() const noexcept
{
    return pImpl->mStartTime;
}

/// Get end time
double TraceBuf2::getEndTime() const
{
    if (!haveSamplingRate())
    {
        throw std::runtime_error("Sampling rate note set");
    }
    if (getNumberOfSamples() < 1)
    {
        throw std::runtime_error("No samples in signal");
    }
    return pImpl->mEndTime;
}

/// Set the sampling rate
void TraceBuf2::setSamplingRate(const double samplingRate)
{
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("samplingRate = "
                                  + std::to_string(samplingRate)
                                  + " must be positive");
    }
    pImpl->mSamplingRate = samplingRate;
    pImpl->updateEndTime();
}

double TraceBuf2::getSamplingRate() const
{
    if (!haveSamplingRate()){throw std::runtime_error("Sampling rate not set");}
    return pImpl->mSamplingRate;
}

bool TraceBuf2::haveSamplingRate() const noexcept
{
    return (pImpl->mSamplingRate > 0);
}

/// Set quality
void TraceBuf2::setQuality(const int quality) noexcept 
{
    // Quality flags in header:
    // AMPLIFIER_SATURATED    0x01 (=1)
    // DIGITIZER_CLIPPED      0x02 (=2)
    // SPIKES_DETECTED        0x04 (=4)
    // GLITCHES_DETECTED      0x08 (=8)
    // MISSING_DATA_PRESENT   0x10 (=16)
    // TELEMETRY_SYNCH_ERROR  0x20 (=20)
    // FILTER_CHARGING        0x40 (=40)
    // TIME_TAG_QUESTIONABLE  0x80 (=80)
    pImpl->mQuality = quality;
}

int TraceBuf2::getQuality() const noexcept
{
    return pImpl->mQuality;
}

/// Get number of samples
int TraceBuf2::getNumberOfSamples() const noexcept
{
    //return static_cast<int> (pImpl->mData.size());
    return pImpl->mSamples;
}

void TraceBuf2::setNumberOfSamples(int nSamples)
{
    if (nSamples < 0)
    {
        throw std::invalid_argument("Number of samples must be non-negative");
    }
    pImpl->mSamples = nSamples;
}

/// Maximum number of samples
/*
int TraceBuf2::getMaximumNumberOfSamples() const noexcept
{
    return pImpl->mMaximumNumberOfSamples;
}
*/

void TraceBuf2::setNativePacket(const char *message,
                                const size_t messageLength)
{
    if (message == nullptr){throw std::runtime_error("message is NULL");}
    std::fill(pImpl->mRawData.begin() + messageLength,
              pImpl->mRawData.end(), 
              '\0');
    std::copy(message, message + messageLength, pImpl->mRawData.begin());
    pImpl->mMessageLength = messageLength;
}

const char *TraceBuf2::getNativePacketPointer() const
{
    if (pImpl->mSamples > 0)
    {
        return pImpl->mRawData.data();
    }
    return nullptr;
} 

size_t TraceBuf2::getMessageLength() const
{
    return pImpl->mMessageLength;
}

/*
/// Set data
template<class T>
template<typename U>
void TraceBuf2<T>::setData(const std::vector<U> &x)
{
    setData(x.size(), x.data());
}

/// Set data
template<class T>
void TraceBuf2<T>::setData(std::vector<T> &&x) noexcept
{
    pImpl->mData = std::move(x); 
    pImpl->updateEndTime();
}

/// Set data
template<class T>
template<typename U>
void TraceBuf2<T>::setData(const int nSamples, const U *__restrict__ x)
{
    // Invalid
    if (nSamples < 0)
    {
        throw std::invalid_argument("nSamples not positive");
    }
    // No data so nothing to do
    pImpl->mData.resize(nSamples);
    pImpl->updateEndTime();
    if (nSamples == 0){return;}
    if (x == nullptr){throw std::invalid_argument("x is NULL");}
    T *__restrict__ dPtr = pImpl->mData.data(); 
    std::copy(x, x + nSamples, dPtr);
}

/// Get data
template<class T>
std::vector<T> TraceBuf2<T>::getData() const noexcept
{
    return pImpl->mData;
}

template<class T>
const T *TraceBuf2<T>::getDataPointer() const
{
    if (pImpl->mData.empty()){throw std::runtime_error("No data set");}
    return pImpl->mData.data();
}
*/

/// Version
std::string TraceBuf2::getVersion() const noexcept
{
    return pImpl->mVersion;
}    

/// Pin number
void TraceBuf2::setPinNumber(const int pinNumber) noexcept
{
    pImpl->mPinNumber = pinNumber;
}

int TraceBuf2::getPinNumber() const noexcept
{
    return pImpl->mPinNumber;
}

/// Destructor
TraceBuf2::~TraceBuf2() = default;

/// Reset class
void TraceBuf2::clear() noexcept
{
    pImpl->clear();
}

/// Serialize
void TraceBuf2::fromEarthworm(const char *message, const size_t messageLength)
{
    auto t = ::unpackEarthwormMessage(message, messageLength);
    *this = std::move(t);
}


///--------------------------------------------------------------------------///
///                          Template Instantiation                          ///
///--------------------------------------------------------------------------///
/*
template class TraceBuf2<double>;
template class TraceBuf2<float>;
template class TraceBuf2<int>;
template class TraceBuf2<int16_t>;
*/

/*
template void TraceBuf2<double>::setData(const std::vector<double> &);
template void TraceBuf2<double>::setData(const std::vector<float> &);
template void TraceBuf2<double>::setData(const std::vector<int> &);
template void TraceBuf2<double>::setData(const std::vector<int8_t> &);
template void TraceBuf2<double>::setData(int nSamples, const double *);
template void TraceBuf2<double>::setData(int nSamples, const float *);
template void TraceBuf2<double>::setData(int nSamples, const int *);
template void TraceBuf2<double>::setData(int nSamples, const int16_t *);

template void TraceBuf2<float>::setData(const std::vector<double> &);
template void TraceBuf2<float>::setData(const std::vector<float> &);
template void TraceBuf2<float>::setData(const std::vector<int> &);
template void TraceBuf2<float>::setData(const std::vector<int8_t> &);
template void TraceBuf2<float>::setData(int nSamples, const double *);
template void TraceBuf2<float>::setData(int nSamples, const float *);
template void TraceBuf2<float>::setData(int nSamples, const int *);
template void TraceBuf2<float>::setData(int nSamples, const int16_t *);

template void TraceBuf2<int>::setData(const std::vector<double> &);
template void TraceBuf2<int>::setData(const std::vector<float> &);
template void TraceBuf2<int>::setData(const std::vector<int> &);
template void TraceBuf2<int>::setData(const std::vector<int8_t> &);
template void TraceBuf2<int>::setData(int nSamples, const double *);
template void TraceBuf2<int>::setData(int nSamples, const float *);
template void TraceBuf2<int>::setData(int nSamples, const int *);
template void TraceBuf2<int>::setData(int nSamples, const int16_t *);

template void TraceBuf2<int16_t>::setData(const std::vector<double> &);
template void TraceBuf2<int16_t>::setData(const std::vector<float> &);
template void TraceBuf2<int16_t>::setData(const std::vector<int> &);
template void TraceBuf2<int16_t>::setData(const std::vector<int8_t> &);
template void TraceBuf2<int16_t>::setData(int nSamples, const double *);
template void TraceBuf2<int16_t>::setData(int nSamples, const float *);
template void TraceBuf2<int16_t>::setData(int nSamples, const int *);
template void TraceBuf2<int16_t>::setData(int nSamples, const int16_t *);
*/
