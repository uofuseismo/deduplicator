#ifndef DEDUPLICATOR_WAVE_RING_HPP
#define DEDUPLICATOR_WAVE_RING_HPP
#include <memory>
#include <string>
#include <vector>
#include <exception>
namespace Deduplicator
{
 class TraceBuf2;
}
namespace Deduplicator
{
class TerminateException : public std::exception
{ 
private: 
    std::string message; 
public: 
    TerminateException(const std::string &msg) :
        message(msg)
    {
    }
    // Constructor accepts a const char* that is used to set 
    // the exception message 
    TerminateException(const char* msg) 
        : message(msg) 
    { 
    } 
    // Override the what() method to return our message 
    const char* what() const throw() 
    { 
        return message.c_str(); 
    } 
}; 
  
/// @class WaveRing "waveRing.hpp" "deduplicator/waveRing.hpp"
/// @brief A utility for reading and writing traceBuf2 messages from an
///        Earthworm wave ring as well as status messages.
/// @copyright Ben Baker (University of Utah) distributed under the MIT license.
class WaveRing
{
public:
    /// @name Constructors
    /// @{

    /// @brief Default constructor.
    WaveRing();
    /// @brief Move constructor.
    /// @param[in,out] waveRing  Initializes the ring reader from this class.
    ///                          On exit, waveRing's behavior is undefined.
    WaveRing(WaveRing &&waveRing) noexcept;
    /// @}

    /// @name Operators
    /// @{

    /// @brief Move assignment operator.
    /// @param[in,out] waveRing  The waveRing class whose memory will be moved
    ///                          to this.  On exit, waveRing's behavior is
    ///                          undefined.
    /// @result The memory from waveRing moved to this.
    WaveRing& operator=(WaveRing &&waveRing) noexcept;
    /// @} 

    /// @name Connection
    /// @{

    /// @result True indicates that UMPS has been compiled with the ability
    ///         to use the earthworm library.
    [[nodiscard]] bool haveEarthworm() const noexcept;
    /// @brief Initializes the ring parameters.
    /// @param[in] ringName          The name of the earthworm ring - e.g.,
    ///                              "WAVE_RING".
    /// @param[in] milliSecondsWait  The number of milliseconds to wait after
    ///                              reading from the earthworm ring.
    /// @throws std::
    void connect(const std::string &ringName, const std::string &moduleName = "");
    /// @result True indicates that this class is connected to an
    ///         earthworm ring.
    [[nodiscard]] bool isConnected() const noexcept;
    /// @result The name of the ring to which this class is attached.
    /// @throws std::runtime_error if \c isConnected() is false.
    [[nodiscard]] std::string getRingName() const;
    /// @}

    /// @name Reading
    /// @{

    /// @brief Flushes the ring.  This is usually a good thing to do on startup.
    /// @throws std::runtime_error if \c isConnected() is false.
    void flush();
    /// @brief Reads the ring.
    /// @throws std::runtime_error if \c isConnected() is false.
    void read();
    void write(const TraceBuf2 &message);
    void writeHeartbeat(bool terminate = false);
    

    /// @result The traceBuf2 messages read from the ring.
    [[nodiscard]] std::vector<TraceBuf2> getTraceBuf2Messages() const noexcept;
    /// @result The number of traceBuf2 messages.
    [[nodiscard]] int getNumberOfTraceBuf2Messages() const noexcept;
    /// @result A pointer to the array of traceBuf2 messages read from the
    ///         ring.  This has dimension [\c getNumberOfTraceBuf2Messages()].
    /// @note This is not recommended for general use.
    [[nodiscard]] const TraceBuf2 *getTraceBuf2MessagesPointer() const noexcept;
    /// @result A reference to the array of traceBuf2 messages read from the
    ///         ring.
    /// @note This is not recommended for general use. 
    [[nodiscard]] const std::vector<TraceBuf2> &getTraceBuf2MessagesReference() const noexcept;
    /// @result The traceBuf2 messages read from the ring moved to this.
    /// @note On exit, all read messages will have been moved and 
    ///       \c getNumberOfTraceBuf2Messages() will be 0.
    [[nodiscard]] std::vector<TraceBuf2> moveTraceBuf2Messages() noexcept;
    /// @}

    /// @name Destructors
    /// @{

    /// @brief Disconnects from the ring.  Additionally, all memory is released.
    void disconnect() noexcept;
    /// @brief Destructor.
    ~WaveRing(); 
    /// @}
 
    WaveRing& operator=(const WaveRing &waveRing) = delete;
    WaveRing(const WaveRing &waveRing) = delete;
private:
    class WaveRingImpl;
    std::unique_ptr<WaveRingImpl> pImpl;
};
}
#endif
