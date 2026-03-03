#pragma once

#include <QString>

namespace LinkForge {
namespace Core {
namespace TypeNames {

// ---------------------------------------------------------------------------
// Transport type names -- canonical tokens used by the factory.
// ---------------------------------------------------------------------------
namespace Transport {
    inline constexpr QLatin1String kSerial   {"Serial"};
    inline constexpr QLatin1String kTcp      {"TCP"};
    inline constexpr QLatin1String kUdp      {"UDP"};
    inline constexpr QLatin1String kCan      {"CAN"};
    inline constexpr QLatin1String kUnknown  {"Unknown"};

    // Aliases recognised by the factory (not canonical output).
    inline constexpr QLatin1String kUart      {"uart"};
    inline constexpr QLatin1String kTcpSocket {"tcpsocket"};
    inline constexpr QLatin1String kUdpSocket {"udpsocket"};
} // namespace Transport

// ---------------------------------------------------------------------------
// Protocol type names.
// ---------------------------------------------------------------------------
namespace Protocol {
    inline constexpr QLatin1String kModbus  {"Modbus"};
    inline constexpr QLatin1String kMavLink {"MAVLink"};
    inline constexpr QLatin1String kUds     {"UDS"};
    inline constexpr QLatin1String kDbc     {"DBC"};
    inline constexpr QLatin1String kCustom  {"Custom"};
    inline constexpr QLatin1String kFrame   {"Frame"};  ///< FrameProtocol
    inline constexpr QLatin1String kUnknown {"Unknown"};
    inline constexpr QLatin1String kJson    {"json"};
    inline constexpr QLatin1String kBinary  {"binary"};
} // namespace Protocol

// ---------------------------------------------------------------------------
// Configuration map keys.
// ---------------------------------------------------------------------------
namespace Config {
    // General
    inline constexpr QLatin1String kTimeout  {"timeout"};
    inline constexpr QLatin1String kMode     {"mode"};
    inline constexpr QLatin1String kSubType  {"subType"};

    // Serial
    inline constexpr QLatin1String kPort        {"port"};
    inline constexpr QLatin1String kBaudRate    {"baudRate"};
    inline constexpr QLatin1String kDataBits    {"dataBits"};
    inline constexpr QLatin1String kParity      {"parity"};
    inline constexpr QLatin1String kStopBits    {"stopBits"};
    inline constexpr QLatin1String kFlowControl {"flowControl"};

    // Network
    inline constexpr QLatin1String kHost        {"host"};
    inline constexpr QLatin1String kLocalHost   {"localHost"};
    inline constexpr QLatin1String kLocalPort   {"localPort"};
    inline constexpr QLatin1String kRemoteHost  {"remoteHost"};
    inline constexpr QLatin1String kRemotePort  {"remotePort"};

    // Modbus
    inline constexpr QLatin1String kSlaveId  {"slaveId"};
    inline constexpr QLatin1String kFunction {"function"};
    inline constexpr QLatin1String kAddress  {"address"};
    inline constexpr QLatin1String kQuantity {"quantity"};
    inline constexpr QLatin1String kValues   {"values"};
} // namespace Config

// ---------------------------------------------------------------------------
// Modbus function-name tokens.
// ---------------------------------------------------------------------------
namespace Modbus {
    inline constexpr QLatin1String kReadCoils              {"readCoils"};
    inline constexpr QLatin1String kReadDiscreteInputs     {"readDiscreteInputs"};
    inline constexpr QLatin1String kReadHoldingRegisters   {"readHoldingRegisters"};
    inline constexpr QLatin1String kReadInputRegisters     {"readInputRegisters"};
    inline constexpr QLatin1String kWriteSingleCoil        {"writeSingleCoil"};
    inline constexpr QLatin1String kWriteSingleRegister    {"writeSingleRegister"};
    inline constexpr QLatin1String kWriteMultipleCoils     {"writeMultipleCoils"};
    inline constexpr QLatin1String kWriteMultipleRegisters {"writeMultipleRegisters"};
    inline constexpr QLatin1String kModeRtu {"RTU"};
    inline constexpr QLatin1String kModeTcp {"TCP"};
} // namespace Modbus

// ---------------------------------------------------------------------------
// Serial-port parameter tokens.
// ---------------------------------------------------------------------------
namespace Serial {
    inline constexpr QLatin1String kParityNone    {"None"};
    inline constexpr QLatin1String kParityEven    {"Even"};
    inline constexpr QLatin1String kParityOdd     {"Odd"};
    inline constexpr QLatin1String kParitySpace   {"Space"};
    inline constexpr QLatin1String kParityMark    {"Mark"};

    inline constexpr QLatin1String kStopBits1     {"1"};
    inline constexpr QLatin1String kStopBits1_5   {"1.5"};
    inline constexpr QLatin1String kStopBits2     {"2"};

    inline constexpr QLatin1String kFlowNone      {"None"};
    inline constexpr QLatin1String kFlowHardware  {"Hardware"};
    inline constexpr QLatin1String kFlowSoftware  {"Software"};
} // namespace Serial

// ---------------------------------------------------------------------------
// CAN bus configuration keys.
// ---------------------------------------------------------------------------
namespace Can {
    /// Qt SerialBus backend name, e.g. "socketcan", "peakcan", "vectorcan".
    inline constexpr QLatin1String kPlugin    {"canPlugin"};
    /// Device interface name, e.g. "can0", "PCAN_USBBUS1".
    inline constexpr QLatin1String kInterface {"canInterface"};
    /// Bit rate in bits/s, e.g. 500000 (500 kbps).
    inline constexpr QLatin1String kBitrate   {"canBitrate"};
    /// Enable CAN-FD frames (bool).
    inline constexpr QLatin1String kFd        {"canFd"};
    /// Enable loopback mode (bool, default false).
    inline constexpr QLatin1String kLoopback  {"canLoopback"};
} // namespace Can

} // namespace TypeNames
} // namespace Core
} // namespace LinkForge
