#pragma once

#include <string_view>  // C++17

namespace LinkForge {
namespace Core {
namespace TypeNames {

// ---------------------------------------------------------------------------
// Transport type names -- canonical tokens used by the factory.
// ---------------------------------------------------------------------------
namespace Transport {
    inline constexpr std::string_view kSerial    = "Serial";
    inline constexpr std::string_view kTcp       = "TCP";
    inline constexpr std::string_view kUdp       = "UDP";
    inline constexpr std::string_view kCan       = "CAN";
    inline constexpr std::string_view kUnknown   = "Unknown";

    // Aliases recognised by the factory (not canonical output).
    inline constexpr std::string_view kUart      = "uart";
    inline constexpr std::string_view kTcpSocket = "tcpsocket";
    inline constexpr std::string_view kUdpSocket = "udpsocket";
} // namespace Transport

// ---------------------------------------------------------------------------
// Protocol type names.
// ---------------------------------------------------------------------------
namespace Protocol {
    inline constexpr std::string_view kModbus  = "Modbus";
    inline constexpr std::string_view kMavLink = "MAVLink";
    inline constexpr std::string_view kUds     = "UDS";
    inline constexpr std::string_view kDbc     = "DBC";
    inline constexpr std::string_view kCustom  = "Custom";
    inline constexpr std::string_view kFrame   = "Frame";  ///< FrameProtocol
    inline constexpr std::string_view kUnknown = "Unknown";
    inline constexpr std::string_view kJson    = "json";
    inline constexpr std::string_view kBinary  = "binary";
} // namespace Protocol

// ---------------------------------------------------------------------------
// Configuration map keys.
// ---------------------------------------------------------------------------
namespace Config {
    // General
    inline constexpr std::string_view kTimeout  = "timeout";
    inline constexpr std::string_view kMode     = "mode";
    inline constexpr std::string_view kSubType  = "subType";

    // Serial
    inline constexpr std::string_view kPort        = "port";
    inline constexpr std::string_view kBaudRate    = "baudRate";
    inline constexpr std::string_view kDataBits    = "dataBits";
    inline constexpr std::string_view kParity      = "parity";
    inline constexpr std::string_view kStopBits    = "stopBits";
    inline constexpr std::string_view kFlowControl = "flowControl";

    // Network
    inline constexpr std::string_view kHost        = "host";
    inline constexpr std::string_view kLocalHost   = "localHost";
    inline constexpr std::string_view kLocalPort   = "localPort";
    inline constexpr std::string_view kRemoteHost  = "remoteHost";
    inline constexpr std::string_view kRemotePort  = "remotePort";

    // Modbus
    inline constexpr std::string_view kSlaveId  = "slaveId";
    inline constexpr std::string_view kFunction = "function";
    inline constexpr std::string_view kAddress  = "address";
    inline constexpr std::string_view kQuantity = "quantity";
    inline constexpr std::string_view kValues   = "values";
} // namespace Config

// ---------------------------------------------------------------------------
// Modbus function-name tokens.
// ---------------------------------------------------------------------------
namespace Modbus {
    inline constexpr std::string_view kReadCoils              = "readCoils";
    inline constexpr std::string_view kReadDiscreteInputs     = "readDiscreteInputs";
    inline constexpr std::string_view kReadHoldingRegisters   = "readHoldingRegisters";
    inline constexpr std::string_view kReadInputRegisters     = "readInputRegisters";
    inline constexpr std::string_view kWriteSingleCoil        = "writeSingleCoil";
    inline constexpr std::string_view kWriteSingleRegister    = "writeSingleRegister";
    inline constexpr std::string_view kWriteMultipleCoils     = "writeMultipleCoils";
    inline constexpr std::string_view kWriteMultipleRegisters = "writeMultipleRegisters";
    inline constexpr std::string_view kModeRtu = "RTU";
    inline constexpr std::string_view kModeTcp = "TCP";
} // namespace Modbus

// ---------------------------------------------------------------------------
// Serial-port parameter tokens.
// ---------------------------------------------------------------------------
namespace Serial {
    inline constexpr std::string_view kParityNone    = "None";
    inline constexpr std::string_view kParityEven    = "Even";
    inline constexpr std::string_view kParityOdd     = "Odd";
    inline constexpr std::string_view kParitySpace   = "Space";
    inline constexpr std::string_view kParityMark    = "Mark";

    inline constexpr std::string_view kStopBits1     = "1";
    inline constexpr std::string_view kStopBits1_5   = "1.5";
    inline constexpr std::string_view kStopBits2     = "2";

    inline constexpr std::string_view kFlowNone      = "None";
    inline constexpr std::string_view kFlowHardware  = "Hardware";
    inline constexpr std::string_view kFlowSoftware  = "Software";
} // namespace Serial

// ---------------------------------------------------------------------------
// CAN bus configuration keys.
// ---------------------------------------------------------------------------
namespace Can {
    /// Qt SerialBus backend name, e.g. "socketcan", "peakcan", "vectorcan".
    inline constexpr std::string_view kPlugin    = "canPlugin";
    /// Device interface name, e.g. "can0", "PCAN_USBBUS1".
    inline constexpr std::string_view kInterface = "canInterface";
    /// Bit rate in bits/s, e.g. 500000 (500 kbps).
    inline constexpr std::string_view kBitrate   = "canBitrate";
    /// Enable CAN-FD frames (bool).
    inline constexpr std::string_view kFd        = "canFd";
    /// Enable loopback mode (bool, default false).
    inline constexpr std::string_view kLoopback  = "canLoopback";
} // namespace Can

} // namespace TypeNames
} // namespace Core
} // namespace LinkForge
