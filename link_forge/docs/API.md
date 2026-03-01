# LinkForge API Reference

## Table of Contents

1. Architecture Overview
2. Naming Conventions
3. Core Layer
   - ITransport
   - IProtocol
   - CommunicationManager
   - ChannelManager
   - TransportFactory
   - ProtocolFactory
4. Plugin System
   - ITransportPlugin
   - IProtocolPlugin
   - PluginManager
5. Transport Implementations
   - SerialTransport
   - TcpSocketTransport
   - UdpSocketTransport
   - CanTransport
6. Protocol Implementations
   - ModbusProtocol
   - CustomProtocol / JsonProtocol
   - FrameProtocol
7. FrameSpec and FrameParser
8. Application Layer
   - MainController
   - MessageModel
9. Utilities
   - Logger
   - ConfigManager
10. Configuration Key Reference
11. Thread Safety
12. Error Handling


---


## 1. Architecture Overview

LinkForge is organized into four horizontal layers.  Each layer depends only on
the layer beneath it; the GUI and application code never touch transport
internals directly.

```
[ GUI / application layer ]   link_forge.h/.cpp, main_controller, message_model
        |
[ Core orchestration     ]   CommunicationManager, ChannelManager, factories
        |
[ Transport / Protocol   ]   Serial, TCP, UDP, CAN, Modbus, Frame, Custom
        |
[ Qt infrastructure      ]   QThread, QSerialPort, QTcpSocket, QCanBusDevice
```

Key design decisions:

- All I/O runs on a dedicated worker thread per channel.  The GUI thread is
  never blocked by serial reads, network connect-timeouts, or CAN frame queues.
- Transport and protocol objects are owned by CommunicationManager, moved to
  its private QThread, and communicated with only through Qt queued connections.
- The factory layer is fully extensible at runtime: third-party plugin DLLs
  can register new transport and protocol types by name.
- Enum-based state machines are used everywhere; no string comparisons at
  runtime for state transitions.


---


## 2. Naming Conventions

All code in this project follows these conventions consistently:

| Category                        | Style                   | Example               |
|---------------------------------|-------------------------|-----------------------|
| Member functions, static funcs  | PascalCase              | BuildMessage()        |
| Qt signals                      | PascalCase              | DataReceived(...)     |
| Member variables                | snake_case_ (trailing _)| receive_buffer_       |
| Local variables, parameters     | snake_case              | frame_size            |
| Compile-time string constants   | kCamelCase              | kSerial, kCan         |
| Enum values                     | PascalCase              | State::Connected      |


---


## 3. Core Layer

### ITransport

Header: `core/itransport.h`
Namespace: `LinkForge::Core`

Abstract base class for every transport implementation.

#### Enumerations

```
ITransport::State
    Disconnected    Initial / after clean shutdown
    Connecting      TCP/UDP only: async connect in progress
    Connected       Ready to send and receive
    Disconnecting   Shutdown in progress
    Error           Unrecoverable error; must re-initialize

ITransport::Type
    Serial
    TcpSocket
    UdpSocket
    Can
    Unknown
```

#### Methods

```
bool Connect(const QVariantMap& config)
```
Initiate a connection with the supplied configuration map.  Serial transports
complete synchronously; TCP/UDP return immediately and report success or failure
through the Connected() and ErrorOccurred() signals.  Returns false only when
the attempt cannot be started (e.g. missing required config key).

```
void Disconnect()
```
Gracefully close the connection.  Always safe to call in any state.

```
qint64 Send(const QByteArray& data)
```
Write bytes to the transport.  Returns the number of bytes queued/written
(>= 0) or -1 on a hard write error.  Must only be called from the I/O thread
(CommunicationManager routes this correctly via QMetaObject::invokeMethod).

```
State   GetState()  const
Type    GetType()   const
QString GetName()   const
QVariantMap GetConfig() const
bool    IsConnected() const
```

#### Signals

```
DataReceived(const QByteArray& data)
StateChanged(ITransport::State new_state)
ErrorOccurred(const QString& error)
Connected()
Disconnected()
```


---


### IProtocol

Header: `core/iprotocol.h`
Namespace: `LinkForge::Core`

Abstract base class for every protocol implementation.

#### IProtocol::Message

```cpp
struct Message {
    QVariantMap payload;       // Structured, typed fields
    QByteArray  raw_data;      // Bytes as sent/received on the wire
    qint64      timestamp_ms;  // QDateTime::currentMSecsSinceEpoch() at parse time
    QString     message_type;  // Human-readable label, e.g. "ModbusRTU", "Frame"
    bool        is_valid;      // True when checksum and framing are correct
};
```

#### Methods

```
std::optional<QByteArray> BuildMessage(const QVariantMap& payload)
```
Encode a structured payload into a wire-format byte array.  Returns nullopt
on encoding failure (e.g. unknown function code, invalid spec).

```
std::vector<Message> ParseData(const QByteArray& data)
```
Feed raw bytes into the protocol parser.  Streaming protocols accumulate
data across calls and may return multiple messages per call, or none if a
frame is not yet complete.

```
bool ValidateMessage(const QByteArray& data) const
```
Check whether a candidate byte array constitutes a valid, complete frame
(correct framing, checksum, length field).  Does not modify internal state.

```
Type    GetType() const
QString GetName() const
void    Reset()
```
Reset flushes all internal reassembly buffers.

#### Signals

```
MessageParsed(const IProtocol::Message& message)
ParseError(const QString& error)
StatusUpdated(const QVariantMap& status)
```


---


### CommunicationManager

Header: `core/communication_manager.h`
Namespace: `LinkForge::Core`

Owns one transport and one protocol.  Runs all I/O on a private QThread.
The owning (GUI) thread communicates with the I/O thread exclusively through
Qt queued connections; it is never blocked.

#### States

```
CommunicationManager::State
    Idle            After Initialize() or Reset()
    Connecting      After Connect() called
    Connected       Transport confirmed connected
    Disconnecting   After Disconnect() called
    Error           Unrecoverable error
```

#### Key Methods

```
bool Initialize(const QString& transport_type,
                const QString& protocol_type,
                const QVariantMap& transport_config,
                const QVariantMap& protocol_config = {})
```
Instantiates the requested transport and protocol through their respective
factories, moves both to the private I/O thread, and starts the thread.
If a previous session exists it is torn down synchronously first.
Returns false when either factory lookup fails.

Recognized transport_type values: "Serial", "TCP", "UDP", "CAN", plus any
name registered by a plugin.

Recognized protocol_type values: "Modbus", "Custom", "Frame", plus any
name registered by a plugin.

```
void Connect()
```
Non-blocking.  Posts a Connect() call to the I/O thread via
QMetaObject::invokeMethod(QueuedConnection).  Result arrives via
StateChanged() and Connected() / ErrorOccurred() signals.

```
void Disconnect()
```
Non-blocking in normal use.  Posts Disconnect() + Reset() to the I/O thread.

```
bool SendMessage(const QVariantMap& payload)
```
Encodes payload with the active protocol and queues the result for async
send.  Returns false when not connected or no protocol is set.  Encoding
and the actual transport write happen on the I/O thread.

```
bool SendRawData(const QByteArray& data)
```
Queues raw bytes for async send without protocol encoding.  Returns false
when not connected.

```
State           GetState()     const
bool            IsConnected()  const
ITransport::Ptr GetTransport() const
IProtocol::Ptr  GetProtocol()  const
void            Reset()
```

#### Signals

```
StateChanged(CommunicationManager::State new_state)
Connected()
Disconnected()
MessageReceived(const IProtocol::Message& message)
RawDataReceived(const QByteArray& data)
DataSent(qint64 bytes)
ErrorOccurred(const QString& error)
```

#### Example

```cpp
auto* mgr = new CommunicationManager(this);

// Initialize Serial + Modbus
QVariantMap t_cfg;
t_cfg["port"]     = "COM3";
t_cfg["baudRate"] = 115200;

QVariantMap p_cfg;
p_cfg["mode"] = "RTU";

if (!mgr->Initialize("Serial", "Modbus", t_cfg, p_cfg)) {
    qWarning() << "init failed";
    return;
}

connect(mgr, &CommunicationManager::MessageReceived, this,
    [](const IProtocol::Message& m) {
        qDebug() << "received" << m.raw_data.toHex();
    });

mgr->Connect();
```


---


### ChannelManager

Header: `core/channel_manager.h`
Namespace: `LinkForge::Core`

Manages a named collection of CommunicationManager instances.  Each channel
has its own transport, protocol, and I/O thread, allowing Serial/Modbus,
TCP/JSON, and CAN/Frame to all run simultaneously without interference.

#### Key Methods

```
bool AddChannel(const QString&     channel_id,
                const QString&     transport_type,
                const QString&     protocol_type,
                const QVariantMap& transport_config,
                const QVariantMap& protocol_config = {})
```
Creates a new channel.  Returns false if the id already exists or factory
lookup fails.

```
bool UpdateChannel(const QString& channel_id, ...)
```
Tears down the existing channel and re-initializes it with new parameters.

```
void RemoveChannel(const QString& channel_id)
void RemoveAllChannels()
void Connect(const QString& channel_id)
void Disconnect(const QString& channel_id)
void ConnectAll()
void DisconnectAll()
bool SendMessage(const QString& channel_id, const QVariantMap& payload)
bool SendRawData(const QString& channel_id, const QByteArray& data)
```

```
bool                    HasChannel(const QString& channel_id)    const
QStringList             ChannelIds()                              const
int                     ChannelCount()                            const
CommunicationManager*   GetChannel(const QString& channel_id)    const
CommunicationManager::State ChannelState(const QString& channel_id) const
```

#### Signals

```
ChannelAdded(const QString& channel_id)
ChannelRemoved(const QString& channel_id)
ChannelStateChanged(const QString& channel_id, CommunicationManager::State state)
ChannelMessageReceived(const QString& channel_id, const IProtocol::Message& message)
ChannelRawDataReceived(const QString& channel_id, const QByteArray& data)
ChannelDataSent(const QString& channel_id, qint64 bytes)
ChannelError(const QString& channel_id, const QString& error)
```

#### Example

```cpp
ChannelManager ch;

// Channel 1: Serial Modbus
ch.AddChannel("plc", "Serial", "Modbus",
    {{"port","COM3"}, {"baudRate",115200}},
    {{"mode","RTU"}});

// Channel 2: CAN + custom frame protocol
QVariantMap frame_cfg;
frame_cfg["headerMagic"]       = "AA55";
frame_cfg["lengthFieldOffset"] = 4;
frame_cfg["lengthFieldSize"]   = 2;
frame_cfg["dataOffset"]        = 6;
frame_cfg["checksumAlgorithm"] = "crc16ibm";

ch.AddChannel("can0", "CAN", "Frame",
    {{"canPlugin","socketcan"}, {"canInterface","can0"}},
    frame_cfg);

connect(&ch, &ChannelManager::ChannelMessageReceived, this,
    [](const QString& id, const IProtocol::Message& msg) {
        qDebug() << id << msg.message_type;
    });

ch.ConnectAll();
```


---


### TransportFactory

Header: `core/transport_factory.h`
Namespace: `LinkForge::Core`

Static factory.  All methods are thread-safe (internal static maps are
initialized once at first call; subsequent calls read without locking).

#### Methods

```
ITransport::Ptr Create(ITransport::Type type, QObject* parent = nullptr)
ITransport::Ptr Create(const QString& type_name, QObject* parent = nullptr)
```
Create(QString) first checks the built-in enum map, then falls back to the
string-keyed plugin map, so plugin-registered names work transparently.

```
void RegisterTransport(ITransport::Type type, CreatorFunc creator)
void RegisterTransport(const QString& type_name, CreatorFunc creator)
```
The QString overload is intended for plugins registering dynamic types.

```
QStringList AvailableTypes()
```
Returns names from both the enum map and the string map.

Default registered types: Serial, TCP, UDP, CAN.


---


### ProtocolFactory

Header: `core/protocol_factory.h`
Namespace: `LinkForge::Core`

Same design as TransportFactory.

Default registered types: Modbus, Custom (binary + JSON), Frame.

```
IProtocol::Ptr Create(IProtocol::Type type,
                       const QVariantMap& config = {},
                       QObject* parent = nullptr)

IProtocol::Ptr Create(const QString& type_name,
                       const QVariantMap& config = {},
                       QObject* parent = nullptr)

void RegisterProtocol(IProtocol::Type type, CreatorFunc creator)
void RegisterProtocol(const QString& type_name, CreatorFunc creator)

QStringList AvailableTypes()
```


---


## 4. Plugin System

### ITransportPlugin

Header: `core/itransport_plugin.h`
IID: `"LinkForge.Core.ITransportPlugin/1.0"`

Interface that a plugin DLL must implement to provide new transport types.

```cpp
class MyPlugin : public QObject, public ITransportPlugin {
    Q_OBJECT
    Q_INTERFACES(LinkForge::Core::ITransportPlugin)
    Q_PLUGIN_METADATA(IID ITransportPlugin_IID FILE "plugin.json")

public:
    QString     PluginName()    const override { return "VectorCAN"; }
    QString     PluginVersion() const override { return "1.0.0"; }
    QStringList SupportedTypes() const override { return {"VectorCAN"}; }

    QString     TypeDescription(const QString&)  const override { ... }
    QVariantMap DefaultConfig(const QString&)    const override { ... }

    ITransport::Ptr CreateTransport(const QString& type_name,
                                     QObject* parent) const override { ... }
};
```

The `plugin.json` file must contain at minimum:

```json
{ "IID": "LinkForge.Core.ITransportPlugin/1.0", "name": "VectorCAN" }
```


---


### IProtocolPlugin

Header: `core/iprotocol_plugin.h`
IID: `"LinkForge.Core.IProtocolPlugin/1.0"`

Same pattern as ITransportPlugin.

```
QString     PluginName()    const
QString     PluginVersion() const
QStringList SupportedTypes() const
QString     TypeDescription(const QString& type_name) const
QVariantMap DefaultConfig(const QString& type_name)   const

IProtocol::Ptr CreateProtocol(const QString&     type_name,
                                const QVariantMap& config,
                                QObject*           parent) const
```


---


### PluginManager

Header: `core/plugin_manager.h`
Namespace: `LinkForge::Core`

Singleton.  Scans directories or loads individual plugin files, probes for
ITransportPlugin and IProtocolPlugin interfaces, and auto-registers all
supported types into the factories.

```
static PluginManager& Instance()

int  LoadPluginsFromDir(const QString& dir_path)
bool LoadPlugin(const QString& file_path)
void UnloadAll()

QStringList LoadedPluginFiles()        const
QStringList RegisteredTransportTypes() const
QStringList RegisteredProtocolTypes()  const
```

Signals:
```
TransportPluginLoaded(const QString& transport_type)
ProtocolPluginLoaded(const QString& protocol_type)
PluginLoadError(const QString& file_path, const QString& error)
```

Example:
```cpp
// At application startup
int n = PluginManager::Instance().LoadPluginsFromDir("plugins/");
qInfo() << "Loaded" << n << "plugin(s)";

// Now "VectorCAN" works transparently:
auto t = TransportFactory::Create("VectorCAN");
```


---


## 5. Transport Implementations

### SerialTransport

Header: `transport/serial_transport.h`

Wraps QSerialPort.  Connect() opens the port synchronously.

Configuration keys:

| Key          | Type    | Default  | Description                              |
|--------------|---------|----------|------------------------------------------|
| port         | string  | required | Port name: "COM3" or "/dev/ttyUSB0"      |
| baudRate     | int     | 9600     | Baud rate                                |
| dataBits     | int     | 8        | 5, 6, 7, or 8                            |
| parity       | string  | "None"   | "None", "Even", "Odd", "Space", "Mark"   |
| stopBits     | string  | "1"      | "1", "1.5", "2"                          |
| flowControl  | string  | "None"   | "None", "Hardware", "Software"           |


---


### TcpSocketTransport

Header: `transport/socket_transport.h`

Non-blocking.  Connect() calls QTcpSocket::connectToHost() and returns
immediately.  The Connected() signal fires when the TCP handshake completes.

Configuration keys:

| Key   | Type   | Default     | Description      |
|-------|--------|-------------|------------------|
| host  | string | "127.0.0.1" | Remote host/IP   |
| port  | int    | 502         | Remote port      |


---


### UdpSocketTransport

Header: `transport/socket_transport.h`

Binds a local endpoint and sends datagrams to a configurable remote endpoint.

Configuration keys:

| Key         | Type   | Default     | Description      |
|-------------|--------|-------------|------------------|
| localHost   | string | "0.0.0.0"   | Local bind address |
| localPort   | int    | 0           | Local bind port (0 = auto) |
| remoteHost  | string | "127.0.0.1" | Destination host |
| remotePort  | int    | 4000        | Destination port |


---


### CanTransport

Header: `transport/can_transport.h`

Wraps QCanBusDevice from Qt SerialBus.  Works with every Qt CAN backend
(socketcan, peakcan, vectorcan, etc.).

Configuration keys:

| Key          | Type   | Default      | Description                                    |
|--------------|--------|--------------|------------------------------------------------|
| canPlugin    | string | "socketcan"  | Qt SerialBus backend name                      |
| canInterface | string | "can0"       | Device interface name                          |
| canBitrate   | uint   | 500000       | Bit rate in bits/s; ignored for virtual CAN    |
| canFd        | bool   | false        | Enable CAN-FD frames                           |
| canLoopback  | bool   | false        | Enable loopback mode                           |

Wire format (QByteArray sent/received on DataReceived / Send):

```
Bytes 0-3  : Frame ID (big-endian uint32)
Byte  4    : Flags (bit 0 = extended frame, bit 1 = RTR, bit 2 = FD bitrate switch)
Byte  5    : Payload length (DLC, 0-8 classic or 0-64 FD)
Bytes 6+   : Payload
```

Static helper methods:
```
QByteArray   SerialiseFrame  (const QCanBusFrame& frame)
QCanBusFrame DeserialiseFrame(const QByteArray& data)
```


---


## 6. Protocol Implementations

### ModbusProtocol

Header: `protocol/modbus_protocol.h`

Implements Modbus RTU and TCP.  Select the mode through the config map:

```cpp
QVariantMap cfg;
cfg["mode"] = "RTU";  // or "TCP"
auto p = ProtocolFactory::Create("Modbus", cfg);
```

BuildMessage() payload keys:

| Key       | Type         | Description                                      |
|-----------|--------------|--------------------------------------------------|
| slaveId   | int          | Station address (RTU only)                       |
| function  | string       | See function names below                         |
| address   | int          | Register or coil start address                   |
| quantity  | int          | Number of registers/coils to read                |
| values    | QVariantList | Values to write (write functions only)           |

Supported function names:

```
readCoils
readDiscreteInputs
readHoldingRegisters
readInputRegisters
writeSingleCoil
writeSingleRegister
writeMultipleCoils
writeMultipleRegisters
```

ParseData() returns one Message per complete response frame.  CRC16/IBM
(RTU) or TCP MBAP header (TCP) is verified; is_valid is false on mismatch.


---


### CustomProtocol / JsonProtocol

Header: `protocol/custom_protocol.h`

Two lightweight implementations registered under the "Custom" type.

CustomProtocol (binary): binary frame with 0xAA55 header, 1-byte type, 2-byte
length, payload, 1-byte checksum.

JsonProtocol: newline-delimited JSON.  Each line is decoded as a QJsonObject
and placed in Message::payload.  Select via the "subType" config key:

```cpp
QVariantMap cfg;
cfg["subType"] = "json";  // omit or set to "binary" for CustomProtocol
auto p = ProtocolFactory::Create("Custom", cfg);
```


---


### FrameProtocol

Header: `protocol/frame_protocol.h`

The primary mechanism for custom serial data frame protocol parsing.  Accepts a
FrameSpec at construction time (or from a QVariantMap via FromConfig()) and
delegates all parsing to an internal FrameParser.

```
static std::shared_ptr<FrameProtocol> FromConfig(const QVariantMap& config,
                                                   QObject* parent = nullptr)
void SetSpec(FrameSpec spec)
const FrameSpec& GetSpec() const
```

See section 7 (FrameSpec and FrameParser) for full configuration details.

To register a FrameProtocol instance with a particular spec in the factory:

```cpp
FrameSpec my_spec = ...;
ProtocolFactory::RegisterProtocol("MyDevice",
    [my_spec](const QVariantMap&, QObject* p) {
        return std::make_shared<FrameProtocol>(my_spec, p);
    });
```

After registration, "MyDevice" can be passed to CommunicationManager::Initialize()
or ChannelManager::AddChannel() like any built-in protocol type.


---


## 7. FrameSpec and FrameParser

### FrameSpec

Header: `protocol/frame_parser.h`

Describes the binary structure of a serial frame.  Covers the vast majority of
embedded device wire formats:

```
[ header_magic ] [ named_fields... ] [ length_field ] [ data ] [ checksum ] [ footer_magic ]
```

All multi-byte integers are big-endian by default.  Set the corresponding
`*_big_endian` flag to false for little-endian fields.

#### FrameSpec fields

Header / footer:

| Field        | Type       | Default | Description                                   |
|--------------|------------|---------|-----------------------------------------------|
| header_magic | QByteArray | empty   | Required byte sequence at frame start         |
| footer_magic | QByteArray | empty   | Required byte sequence at frame end           |

Named fixed fields (decoded into Message::payload):

| Field        | Type                      | Description                                     |
|--------------|---------------------------|-------------------------------------------------|
| named_fields | vector<NamedField>        | Fixed-size header fields decoded by name        |

NamedField members:

| Member    | Type    | Description                          |
|-----------|---------|--------------------------------------|
| name      | QString | Key used in Message::payload         |
| offset    | int     | Byte offset from frame start         |
| size      | int     | Field width in bytes (1-4)           |
| big_endian| bool    | Endianness of this field             |

Length field:

| Field                 | Type | Default | Description                                       |
|-----------------------|------|---------|---------------------------------------------------|
| length_field_offset   | int  | -1      | -1 means fixed-size; use fixed_data_size instead  |
| length_field_size     | int  | 2       | Width of length field in bytes (1, 2, or 4)       |
| length_big_endian     | bool | true    |                                                   |
| length_adjustment     | int  | 0       | data_bytes = decoded_length + length_adjustment   |

Data payload:

| Field           | Type | Default | Description                                          |
|-----------------|------|---------|------------------------------------------------------|
| data_offset     | int  | 0       | Byte offset where the data payload starts            |
| fixed_data_size | int  | -1      | Payload size for fixed-size frames (-1 = variable)   |

Checksum:

| Field                  | Type              | Default | Description                                  |
|------------------------|-------------------|---------|----------------------------------------------|
| checksum_algorithm     | ChecksumAlgorithm | None    | See below                                    |
| checksum_size          | int               | 2       | Width of checksum field in bytes (1-4)       |
| checksum_big_endian    | bool              | true    |                                              |
| checksum_range_begin   | int               | 0       | First covered byte (from frame start)        |
| checksum_range_end     | int               | -1      | Last covered byte; -1 = up to checksum field |

Checksum algorithms (FrameSpec::ChecksumAlgorithm):

| Value      | Description                                    |
|------------|------------------------------------------------|
| None       | No checksum                                    |
| Sum8       | 8-bit modular sum                              |
| Sum16      | 16-bit modular sum                             |
| CRC16_IBM  | CRC-16/IBM, poly 0x8005, Modbus RTU            |
| CRC16_CCITT| CRC-16/CCITT, poly 0x1021, X.25/PPP           |
| CRC32      | IEEE 802.3 CRC-32                              |

#### FrameSpec helper methods

```
int  FrameSize(int data_bytes) const   // Total frame size for given payload length
int  MinFrameSize()            const   // Minimum bytes to attempt parsing
bool IsValid()                 const   // Basic consistency check
```

#### Example: AA55 / addr / cmd / len(2) / data / CRC16

```cpp
FrameSpec spec;
spec.header_magic        = QByteArray::fromHex("AA55");
spec.named_fields        = {
    {"addr", 2, 1},   // 1 byte at offset 2
    {"cmd",  3, 1},   // 1 byte at offset 3
};
spec.length_field_offset = 4;
spec.length_field_size   = 2;
spec.data_offset         = 6;
spec.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::CRC16_IBM;
spec.checksum_size       = 2;
```

For a 4-byte payload this produces a 14-byte frame:
AA 55 | addr | cmd | 00 04 | [4 bytes data] | [CRC16 2 bytes]

#### FrameSpec via QVariantMap (for runtime/JSON configuration)

```
Key                  Type      Default   Description
headerMagic          string    ""        Hex-encoded, e.g. "AA55"
footerMagic          string    ""        Hex-encoded
lengthFieldOffset    int       -1
lengthFieldSize      int       2
lengthBigEndian      bool      true
lengthAdjustment     int       0
dataOffset           int       0
fixedDataSize        int       -1
checksumAlgorithm    string    "none"    "none","sum8","sum16","crc16ibm","crc16ccitt","crc32"
checksumSize         int       2
checksumBigEndian    bool      true
checksumRangeBegin   int       0
checksumRangeEnd     int       -1
namedFields          list      []        List of {name, offset, size, bigEndian} objects
```


---


### FrameParser

Header: `protocol/frame_parser.h`

Stateful streaming parser.  Feed partial or multi-frame byte arrays
incrementally; complete frames are returned as IProtocol::Message objects.

```
explicit FrameParser(FrameSpec spec)

std::vector<IProtocol::Message> Feed(const QByteArray& data)
void Reset()
void SetSpec(FrameSpec spec)
const FrameSpec& GetSpec() const
```

Static checksum utilities:
```
static quint32 ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                const QByteArray& data)
static quint32 ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                const QByteArray& data, int begin, int end)
```

Message fields populated by FrameParser:

| Field        | Value                                                  |
|--------------|--------------------------------------------------------|
| raw_data     | Complete raw frame bytes                               |
| is_valid     | true when checksum and framing pass                    |
| message_type | "Frame"                                                |
| timestamp_ms | QDateTime::currentMSecsSinceEpoch()                   |
| payload["data"] | Variable-length data portion as QByteArray          |
| payload[field.name] | Decoded value of each NamedField as quint32    |


---


## 8. Application Layer

### MainController

Header: `application/main_controller.h`

Bridge between the GUI and a single CommunicationManager instance.

```
bool  InitializeCommunication(const QString& transport_type,
                               const QString& protocol_type,
                               const QVariantMap& transport_config,
                               const QVariantMap& protocol_config = {})
void  Connect()
void  Disconnect()
bool  SendMessage(const QVariantMap& payload)
bool  SendRawData(const QByteArray& data)
bool  IsConnected()        const
QVariantMap GetStatistics() const
MessageModel* GetMessageModel() const
```

GetStatistics() returns a QVariantMap with keys:
bytesSent, bytesReceived, messagesSent, messagesReceived.

Signals:
```
ConnectionStateChanged(bool connected)
MessageReceived(const QString& message_type)
StatisticsUpdated(const QVariantMap& stats)
ErrorOccurred(const QString& error)
```


---


### MessageModel

Header: `application/message_model.h`

QAbstractTableModel subclass suitable for direct use with QTableView.
Stores transmitted and received messages in a circular buffer of configurable
maximum size (default 1000 messages).

Columns: Timestamp, Direction (TX/RX), Type, Size, Data (hex preview).

```
void AddReceivedMessage(const IProtocol::Message& message)
void AddSentMessage(const QVariantMap& payload, const QByteArray& raw)
void Clear()
void SetMaxMessages(int max)
int  GetMessageCount() const
```


---


## 9. Utilities

### Logger

Header: `utils/Logger.h`

Thread-safe singleton logger.  Optionally redirects Qt's built-in message
handler so qDebug/qWarning/qCritical calls also go through Logger.

```
static Logger& Instance()

void Install()                          // Call qInstallMessageHandler
bool EnableFileLogging(const QString& file_path)

void Debug   (const QString& msg, const QString& category = "General")
void Info    (const QString& msg, const QString& category = "General")
void Warning (const QString& msg, const QString& category = "General")
void Error   (const QString& msg, const QString& category = "General")
void Critical(const QString& msg, const QString& category = "General")
```

Signal:
```
LogMessageGenerated(const QString& formatted_message)
```

Example:
```cpp
Logger::Instance().Install();
Logger::Instance().EnableFileLogging("app.log");
Logger::Instance().Info("Application started", "Main");
```


---


### ConfigManager

Header: `utils/ConfigManager.h`

Singleton JSON configuration store.  Values are keyed by dot-separated
paths ("transport.serial.port") and persist to a JSON file.

```
static ConfigManager& Instance()

bool LoadConfig(const QString& file_path)
bool SaveConfig(const QString& file_path = {})

QVariant GetValue(const QString& key, const QVariant& default_value = {}) const
void     SetValue(const QString& key, const QVariant& value)

QVariantMap GetTransportConfig(const QString& transport_type) const
void        SetTransportConfig(const QString& transport_type, const QVariantMap& config)

QVariantMap GetProtocolConfig(const QString& protocol_type) const
void        SetProtocolConfig(const QString& protocol_type, const QVariantMap& config)
```


---


## 10. Configuration Key Reference

All compile-time string constants are in `core/type_names.h` under
`LinkForge::Core::TypeNames`.

Transport type names (TypeNames::Transport):

| Constant    | Value       |
|-------------|-------------|
| kSerial     | "Serial"    |
| kTcp        | "TCP"       |
| kUdp        | "UDP"       |
| kCan        | "CAN"       |

Protocol type names (TypeNames::Protocol):

| Constant    | Value       |
|-------------|-------------|
| kModbus     | "Modbus"    |
| kCustom     | "Custom"    |
| kFrame      | "Frame"     |

CAN config keys (TypeNames::Can):

| Constant     | Value          |
|--------------|----------------|
| kPlugin      | "canPlugin"    |
| kInterface   | "canInterface" |
| kBitrate     | "canBitrate"   |
| kFd          | "canFd"        |
| kLoopback    | "canLoopback"  |

Serial config keys (TypeNames::Config):

| Constant     | Value          |
|--------------|----------------|
| kPort        | "port"         |
| kBaudRate    | "baudRate"     |
| kDataBits    | "dataBits"     |
| kParity      | "parity"       |
| kStopBits    | "stopBits"     |
| kFlowControl | "flowControl"  |


---


## 11. Thread Safety

| Class                | Thread safety notes                                              |
|----------------------|------------------------------------------------------------------|
| CommunicationManager | All public methods safe from the owning (GUI) thread.  Transport and protocol run on io_thread_; never call them directly. |
| ChannelManager       | AddChannel / RemoveChannel / query methods guarded by QMutex; safe from any thread. Connect/Send must be called from the owning thread. |
| FrameParser          | Not thread-safe; guard externally if shared, or use one instance per thread. FrameProtocol guards its internal parser with QMutex. |
| PluginManager        | Instance() and all public methods guarded by QMutex; safe from any thread. |
| TransportFactory     | Static maps initialized once (call_once semantic); safe to call Create() from any thread after initialization. |
| ProtocolFactory      | Same as TransportFactory.                                        |
| Logger               | Thread-safe; uses QMutex internally.                             |
| ConfigManager        | Not thread-safe; use from the main thread only.                  |


---


## 12. Error Handling

All errors are non-throwing.  Error conditions are communicated through:

1. Return values: methods that can fail return bool or std::optional.
2. Signals: ErrorOccurred(QString) emitted by CommunicationManager,
   ChannelManager (as ChannelError), and individual transport/protocol objects.
3. Qt logging: qWarning() / qCritical() with a class-prefixed message, visible
   in the debug output and captured by Logger if Install() was called.

When CommunicationManager::Initialize() returns false, neither the transport
nor the protocol pointer is set and the I/O thread is not started; it is safe
to call Initialize() again immediately with corrected parameters.

When a transport enters State::Error, call Reset() on CommunicationManager (or
remove and re-add the channel via ChannelManager) before attempting to reconnect.
