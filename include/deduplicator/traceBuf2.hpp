#ifndef DEDUPLICATOR_TRACEBUF2_HPP
#define DEDUPLICATOR_TRACEBUF2_HPP
#include <memory>
#include <vector>
#include <string>
namespace Deduplicator
{
/// @class TraceBuf2 "traceBuf2.hpp" "deduplicator/traceBuf2.hpp"
/// @brief Defines an Earthworm tracebuf2 message format.
/// @copyright Ben Baker (University of Utah) distributed under the MIT license.
class TraceBuf2
{
public:
    /// @name Constructors
    /// @{

    /// @brief Constructor
    TraceBuf2();
    /// @brief Copy constructor.
    /// @param[in] traceBuf2  The tracebuf2 from which to initialize this class.
    TraceBuf2(const TraceBuf2 &traceBuf2);
    /// @brief Move constructor.
    /// @param[in] traceBuf2  The tracebuf2 from which to initialize this class.
    ///                       On exit, tracebuf2's behavior is undefined.
    TraceBuf2(TraceBuf2 &&traceBuf2) noexcept;
    /// @}

    /// @name Operators
    /// @{

    /// @brief Copy assignment.
    /// @param[in] traceBuf2  The traceBuf2 class to copy to this.
    /// @result A deep copy of the input traceBuf2.
    TraceBuf2& operator=(const TraceBuf2 &traceBuf2);
    /// @brief Move assignment.
    /// @param[in,out] traceBuf2  The traceBuf2 class whose memory will be
    ///                           moved to this.  On exit, traceBuf2's 
    ///                           behavior is undefined.
    /// @result The memory from traceBuf2 moved to this.
    TraceBuf2& operator=(TraceBuf2 &&traceBuf2) noexcept;
    /// @}

    /// @name Trace Header Information
    /// @{

    /// @param[in] pinNumber  The pin number.
    void setPinNumber(int pinNumber) noexcept;
    /// @result The pin number.
    [[nodiscard]] int getPinNumber() const noexcept;
    
    /// @brief Sets the start time.
    /// @param[in] startTime   The UTC time of the first sample in seconds from
    ///                        the epoch (January 1 1970). 
    void setStartTime(double startTime) noexcept;
    /// @result The UTC time of the first sample in seconds from the epoch.
    [[nodiscard]] double getStartTime() const noexcept;

    /// @result The UTC time of the last sample in seconds from the epoch.
    /// @throws std::runtime_error if \c haveSamplingRate() is false
    ///         \c getNumberOfSamples() is zero.
    [[nodiscard]] double getEndTime() const;

    /// @brief Sets the sampling rate for the data in the packet.
    /// @param[in] samplingRate   The sampling rate in Hz.
    /// @throws std::invalid_argument if the sampling rate is not positive.
    void setSamplingRate(double samplingRate);
    /// @result The sampling rate in Hz.
    /// @throws std::runtime_error if \c haveSamplingRate() is false.
    [[nodiscard]] double getSamplingRate() const;
    /// @result True indicates that the sampling rate was set.
    [[nodiscard]] bool haveSamplingRate() const noexcept;
    
    /// @result The number of samples.
    /// @note The number of samples is set when setting the data.
    [[nodiscard]] int getNumberOfSamples() const noexcept;

    /// @result The maximum number of samples that can be packed into a message
    ///         and put onto an earthworm ring.
    [[nodiscard]] int getMaximumNumberOfSamples() const noexcept;

    /// @brief Sets the network code.
    /// @param[in] network   The network name.
    /// @note If this is larger that \c getMaximumNetworkLength()
    ///       then it will be truncated.
    void setNetwork(const std::string &network) noexcept;
    /// @result The network name. 
    [[nodiscard]] std::string getNetwork() const noexcept;
    /// @result The maximum network code length.  This is likely 8.
    [[nodiscard]] int getMaximumNetworkLength() const noexcept;

    /// @brief Sets the station name.
    /// @param[in] station  The station name.
    /// @note If this is larger that \c getMaximumStationLength()
    ///       then it will be truncated truncated.
    void setStation(const std::string &station) noexcept;
    /// @result The station name. 
    [[nodiscard]] std::string getStation() const noexcept;
    /// @result The maximum station name length.  This is likely 6.
    [[nodiscard]] int getMaximumStationLength() const noexcept;

    /// @brief Sets the channel name.
    /// @param[in] channel  The channel name.
    /// @note If this is larger that \c getMaximumChannelength()
    ///       then it will be truncated truncated.
    void setChannel(const std::string &channel) noexcept;
    /// @result The channel name. 
    [[nodiscard]] std::string getChannel() const noexcept;
    /// @result The maximum channel name length.  This is likely 3.
    [[nodiscard]] int getMaximumChannelLength() const noexcept;

    /// @brief Sets the location code.
    /// @param[in] location   The location code.
    /// @note If this is larger that \c getMaximumLocationCodeLength()
    ///       then it will be truncated truncated.
    void setLocationCode(const std::string &location) noexcept;
    /// @result The location code.
    [[nodiscard]] std::string getLocationCode() const noexcept;
    /// @result The maximum location code length.  This is likely 2.
    [[nodiscard]] int getMaximumLocationCodeLength() const noexcept;

    /// @result The data type.  This is for earthworm compability.  The 
    ///         underlying template type dictates.
    [[nodiscard]] std::string getDataType() const noexcept;

    /// @brief A data quality indicator.
    /// @param[in] quality  The quality flag.
    /// @note From SEED we have:
    ///       Amplifier saturation detected = 1,
    ///       Digitizer clipping detected = 2,
    ///       Spikes detected = 3,
    ///       Glitches detected = 4,
    ///       Missing/padded data present = 16,
    ///       Telemetry synchronization error = 32,
    ///       A digital filter may be charging = 64,
    ///       Time tag is questionable = 128
    void setQuality(int quality) noexcept;      
    /// @result The quality.
    [[nodiscard]] int getQuality() const noexcept;

    /// @result The version.
    [[nodiscard]] std::string getVersion() const noexcept;

    /// @brief Sets the number of samples.
    void setNumberOfSamples(int nSamples);
    /// @}


    /// @name Data
    /// @{

    /// @param[in,out] data  The trace data to set.  On exit, data's behavior is
    ///                      undefined.
    //void setData(std::vector<T> &&data) noexcept;
    /// @param[in] data  The trace data to set.
    //template<typename U> void setData(const std::vector<U> &data);
    /// @param[in] nSamples  The number of samples.
    /// @param[in] data      The data to set on the class.  This is an array
    ///                      whose dimension is [nSamples].
    //template<typename U> void setData(const int nSamples, const U *data);

    void setNativePacket(const char *message, size_t messageLength);
    /// @result A pointer to the native packet that was read-in.
    ///         This has length \c getMessageLength().
    const char *getNativePacketPointer() const;
    [[nodiscard]] size_t getMessageLength() const;

    /// @result The data in the packet.
    //[[nodiscard]] std::vector<T> getData() const noexcept;
    /// @result A pointer to the data in the packet.  This is an array whose
    ///         dimension is [\c getNumberOfSamples() ].
    /// @throws std::runtime_error if \c haveData() is false.
    /// @note This function exists for performance reasons and it is not
    ///       recommended for general use.
    //[[nodiscard]] const T* getDataPointer() const;
    /// @}

    /// @name Destructors
    /// @{

    /// @brief Resets the class and releases memory.
    void clear() noexcept;
    /// @brief Destructor.
    ~TraceBuf2();
    /// @}

    /// @name (De)serialization
    /// @{

    /// @brief Unpacks a tracebuf2 message from the earthworm ring.
    /// @param[in] message   The earthworm message.
    /// @throws std::runtime_error if the message is invalid or NULL.
    void fromEarthworm(const char *message, const size_t length);

    /// @}

private:
    class TraceBuf2Impl;
    std::unique_ptr<TraceBuf2Impl> pImpl;
};
}
#endif
