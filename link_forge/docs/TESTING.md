# LinkForge Test Suite Reference

## Table of Contents

1. Overview
2. File Layout
3. Build and Run
4. Test Class Index
5. tst_frame_parser.h
   - TestFrameSpec
   - TestFrameParserChecksums
   - TestFrameParserStreaming
   - TestFrameParserNamedFields
   - TestFrameParserInvalidFrames
6. tst_frame_protocol.h
   - TestFrameProtocolRoundTrip
   - TestFrameProtocolFromConfig
   - TestFrameProtocolSignals
   - TestFrameProtocolValidation
7. tst_factories.h
   - TestTransportFactory
   - TestProtocolFactory
   - TestCommunicationManager
   - TestChannelManager
   - TestIProtocolMessage
8. Shared Test Utilities
9. Adding New Tests


---


## 1. Overview

The unit tests are written with the Qt Test framework (QTest).  Each test suite
is a class that inherits QObject and declares its test methods as private slots.
The runner in `main_tests.cpp` instantiates every suite class and calls
QTest::qExec() on each one in sequence.

Design goals:

- No hardware dependency.  All tests that touch CommunicationManager or
  ChannelManager do so through factory-created objects that are never actually
  connected; the state-machine guards are tested at the API boundary.
- A `MockTransport` inner class is defined in `tst_factories.h` and provides a
  fully functional in-process ITransport that can simulate DataReceived and
  ErrorOccurred signals without any OS resource.
- Checksum algorithms are tested against independent reference implementations
  embedded in the test helpers, not just self-consistency checks.
- Streaming behaviour is verified exhaustively: for every possible byte split
  position in a well-formed frame, the parser is expected to return exactly one
  valid result across two Feed() calls.


---


## 2. File Layout

```
tests/
    main_tests.cpp          Entry point; runs all suites
    tests.pro               qmake project file
    tst_frame_parser.h      FrameSpec and FrameParser tests
    tst_frame_protocol.h    FrameProtocol tests
    tst_factories.h         Factory, CommunicationManager, ChannelManager tests
```

All test header files use `#pragma once` and are included directly into
`main_tests.cpp`.  Each header ends with a `.moc` include so Qt's meta-object
system processes the Q_OBJECT macros that appear in the test classes.


---


## 3. Build and Run

### Prerequisites

- Qt 5.15 or Qt 6.x with the following modules: Core, Test, SerialPort,
  SerialBus, Network.
- A C++17-capable compiler (MSVC 2019+, GCC 9+, or Clang 10+).

### Build

```
cd link_forge/tests
qmake tests.pro
make           # Linux / macOS
nmake          # Windows with MSVC
```

The output executable is named `link_forge_tests` (or `link_forge_tests.exe`
on Windows).

### Run all suites

```
./link_forge_tests -v2
```

The `-v2` flag prints every test method name and its pass/fail result.
Without it only failures are printed.

### Run a single suite

```
./link_forge_tests TestFrameParserStreaming -v2
```

### Run a single test method

```
./link_forge_tests TestFrameParserStreaming::TestTwoFramesInOneFeed -v2
```

### Exit code

The process exits with 0 when all tests pass.  A non-zero exit code means
at least one method failed; the count of failures is the exit code value.


---


## 4. Test Class Index

| Class                        | File                   | Methods |
|------------------------------|------------------------|---------|
| TestFrameSpec                | tst_frame_parser.h     | 4       |
| TestFrameParserChecksums     | tst_frame_parser.h     | 7       |
| TestFrameParserStreaming      | tst_frame_parser.h     | 8       |
| TestFrameParserNamedFields   | tst_frame_parser.h     | 5       |
| TestFrameParserInvalidFrames | tst_frame_parser.h     | 4       |
| TestFrameProtocolRoundTrip   | tst_frame_protocol.h   | 7       |
| TestFrameProtocolFromConfig  | tst_frame_protocol.h   | 6       |
| TestFrameProtocolSignals     | tst_frame_protocol.h   | 3       |
| TestFrameProtocolValidation  | tst_frame_protocol.h   | 5       |
| TestTransportFactory         | tst_factories.h        | 11      |
| TestProtocolFactory          | tst_factories.h        | 7       |
| TestCommunicationManager     | tst_factories.h        | 14      |
| TestChannelManager           | tst_factories.h        | 17      |
| TestIProtocolMessage         | tst_factories.h        | 3       |
| Total                        |                        | 101     |


---


## 5. tst_frame_parser.h

### TestFrameSpec

Tests the `FrameSpec` struct's helper methods independently of any parsing.

| Method                               | What is verified                                                    |
|--------------------------------------|---------------------------------------------------------------------|
| TestIsValidReturnsFalseForEmptySpec  | A default-constructed FrameSpec has IsValid() == false              |
| TestIsValidReturnsTrueForWellFormedSpec | The canonical AA55/CRC16 spec passes IsValid()                   |
| TestMinFrameSize                     | For the canonical spec the minimum frame size is 8 bytes (header 2 + addr 1 + cmd 1 + length 2 + data 0 + CRC 2) |
| TestFrameSize                        | FrameSize(n) == MinFrameSize() + n for n = 0 and n = 4             |
| TestFixedSizeSpec                    | A fixed-data-size spec (no length field) is valid and computes the correct total frame size |

### TestFrameParserChecksums

Tests the static `FrameParser::ComputeChecksum()` helper against known reference values.

| Method                     | Algorithm     | What is verified                                                        |
|----------------------------|---------------|-------------------------------------------------------------------------|
| TestCRC16IBM_KnownVector   | CRC16_IBM     | Modbus test vector: 0x01 0x03 0x00 0x00 0x00 0x02 produces 0xC40B      |
| TestCRC16IBM_SubRange      | CRC16_IBM     | Computing over a sub-range [1,7) of a 8-byte array matches the full computation over the inner 6 bytes |
| TestSum8                   | Sum8          | 0x01+0x02+0x03+0x04 = 0x0A                                              |
| TestSum8Overflow           | Sum8          | Four 0xFF bytes (sum = 1020) wraps to 0xFC                              |
| TestSum16                  | Sum16         | Same data as Sum8 but result is 16-bit without overflow                 |
| TestNone                   | None          | Returns 0 regardless of input data                                      |
| TestCRC32SelfConsistency   | CRC32         | Two calls with identical data return identical non-zero values           |

### TestFrameParserStreaming

Tests the incremental byte-stream reassembly logic.

| Method                   | What is verified                                                                           |
|--------------------------|--------------------------------------------------------------------------------------------|
| TestSingleFrame          | A single complete frame fed in one call returns one valid Message with correct raw_data and message_type |
| TestAllSplitPositions    | The frame is split at every possible byte boundary (1 to size-1); exactly one valid Message is produced across the two Feed() calls |
| TestTwoFramesInOneFeed   | Two back-to-back frames in one Feed() call returns two valid Messages                      |
| TestThreeFrames          | Three back-to-back frames returns exactly three results                                    |
| TestEmptyFeed            | Feeding an empty QByteArray returns an empty result without crashing                       |
| TestGarbageBeforeFrame   | Six arbitrary garbage bytes prepended to a valid frame; the parser skips them and finds the frame |
| TestResetFlushesBuffer   | After feeding the first half of a frame and calling Reset(), feeding the second half does not produce a valid frame |
| TestIncrementalFeed      | A frame fed in five roughly equal chunks produces exactly one valid Message                |

### TestFrameParserNamedFields

Tests decoding of named fixed header fields into `Message::payload`.

| Method               | What is verified                                                                      |
|----------------------|---------------------------------------------------------------------------------------|
| TestAddrAndCmdDecoded | The `addr` and `cmd` fields (1 byte each at offsets 2 and 3) are decoded correctly into payload["addr"] and payload["cmd"] |
| TestDataPayloadKey   | The variable-length payload portion appears under payload["data"] as a QByteArray     |
| TestUInt16BEField    | A 2-byte big-endian field at offset 2 (value 0x1388 = 5000) is decoded correctly      |
| TestUInt16LEField    | A 2-byte little-endian field (stored as 0x88 0x13, value 5000) is decoded correctly   |
| TestEmptyDataPayload | A frame with a zero-length data section is valid and payload["data"] is an empty QByteArray |

### TestFrameParserInvalidFrames

Tests how the parser handles malformed or incomplete input.

| Method                | What is verified                                                                                         |
|-----------------------|----------------------------------------------------------------------------------------------------------|
| TestWrongChecksum     | A frame whose last CRC byte is flipped either produces is_valid == false or is discarded (empty result) |
| TestTruncatedFrame    | Feeding a frame minus its last byte produces no valid result                                             |
| TestWrongHeaderMagic  | A frame whose first magic byte is set to 0x00 is not returned as a valid frame                          |
| TestSetSpecReplacesSpec | Calling SetSpec() with a new header magic updates GetSpec() to the new value                          |


---


## 6. tst_frame_protocol.h

### TestFrameProtocolRoundTrip

Tests that BuildMessage() followed by ParseData() recovers the original payload.

| Method                      | What is verified                                                                              |
|-----------------------------|-----------------------------------------------------------------------------------------------|
| TestBuildThenParseDataPayload | A 4-byte hex payload encodes and decodes back to the same bytes under payload["data"]        |
| TestRoundTripEmptyData       | An empty data payload builds and parses without error; is_valid is true                      |
| TestRoundTripLargePayload    | A 1024-byte payload round-trips without corruption                                            |
| TestTwoConsecutiveRoundTrips | Two independent encode/decode cycles on the same FrameProtocol instance both succeed        |
| TestGetTypeAndName           | GetName() returns "Frame"                                                                     |
| TestMessageTypeField         | The parsed Message::message_type field is "Frame"                                             |
| TestTimestampPopulated       | Message::timestamp_ms is greater than 0 after parsing                                        |

### TestFrameProtocolFromConfig

Tests construction of a FrameProtocol from a QVariantMap (as used by the protocol factory).

| Method                         | What is verified                                                                         |
|--------------------------------|------------------------------------------------------------------------------------------|
| TestFromConfigReturnsNonNull   | A valid config map produces a non-null FrameProtocol pointer                             |
| TestFromConfigSpecValues       | header_magic, length_field_offset, length_field_size, data_offset, checksum_algorithm, and checksum_size match the config values |
| TestFromConfigChecksumAlgorithms | All six algorithm strings ("none", "sum8", "sum16", "crc16ibm", "crc16ccitt", "crc32") are parsed to the correct enum value |
| TestFromConfigNamedFields      | A two-entry namedFields list is parsed; the resulting FrameSpec has the correct field names |
| TestFactoryCreateFrame         | ProtocolFactory::Create("Frame", config) returns a non-null object with GetName() == "Frame" |
| TestFromConfigRoundTrip        | A FrameProtocol constructed from config performs a successful encode/decode round-trip    |

### TestFrameProtocolSignals

Tests the Qt signals emitted by FrameProtocol.

| Method                           | What is verified                                                                                          |
|----------------------------------|-----------------------------------------------------------------------------------------------------------|
| TestMessageParsedSignal          | Feeding one valid frame causes IProtocol::MessageParsed to be emitted once; the message's is_valid is true |
| TestMessageParsedSignalTwoFrames | Two frames fed in one call causes MessageParsed to be emitted twice                                       |
| TestParseErrorSignalOnBadChecksum | Feeding a frame with a flipped CRC byte either emits ParseError, or MessageParsed is not emitted with is_valid == true |

### TestFrameProtocolValidation

Tests `ValidateMessage()` and the `Reset()` / `SetSpec()` state management methods.

| Method                       | What is verified                                                                    |
|------------------------------|-------------------------------------------------------------------------------------|
| TestValidateGoodFrame        | A correctly formed frame returns true from ValidateMessage()                        |
| TestValidateWrongMagic       | A frame with a corrupted first magic byte returns false                             |
| TestValidateWrongChecksum    | A frame with a flipped CRC byte returns false                                       |
| TestValidateDoesNotAffectState | Calling ValidateMessage() does not disturb the internal streaming buffer; ParseData() still works correctly afterward |
| TestResetClearsStreamBuffer  | Feeding the first half, calling Reset(), then feeding the second half produces no valid output |
| TestSetSpecReplacesSpec      | SetSpec() with a new header_magic updates GetSpec() to the new value               |


---


## 7. tst_factories.h

### MockTransport

A concrete ITransport defined inside `tst_factories.h` for use in
CommunicationManager tests.  It does not open any OS resource.

Behaviour:
- `Connect()` sets the state to Connected and emits Connected() and StateChanged().
- `Disconnect()` sets the state to Disconnected and emits Disconnected() and StateChanged().
- `Send()` stores the bytes in `last_sent_` and increments `sent_count_`.
- `SimulateReceive(data)` emits DataReceived(data).
- `SimulateError(msg)` emits ErrorOccurred(msg) and sets state to Error.
- `LastSent()`, `SentCount()`, and `ResetCounters()` provide inspection helpers.

### TestTransportFactory

| Method                              | What is verified                                                                  |
|-------------------------------------|-----------------------------------------------------------------------------------|
| TestCreateSerial                    | Create(Type::Serial) returns a non-null object of type Serial                     |
| TestCreateTcp                       | Create(Type::TcpSocket) returns a non-null object of type TcpSocket               |
| TestCreateUdp                       | Create(Type::UdpSocket) returns a non-null object of type UdpSocket               |
| TestCreateCan                       | Create(Type::Can) returns a non-null object of type Can                           |
| TestCreateByStringSerial            | Create("Serial") returns a non-null object                                        |
| TestCreateByStringTcp               | Create("TCP") returns a non-null object                                           |
| TestCreateByStringUdp               | Create("UDP") returns a non-null object                                           |
| TestCreateByStringCan               | Create("CAN") returns a non-null object                                           |
| TestCreateUnknownString             | Create("NoSuchTransport_xyz") returns nullptr without crashing                    |
| TestCreateUnknownEnum               | Create(Type::Unknown) returns nullptr without crashing                            |
| TestRegisterAndCreateByStringKey    | RegisterTransport("MockBus_test", fn) followed by Create("MockBus_test") returns the MockTransport |
| TestRegisterDuplicateKey            | Registering the same string key twice does not crash; Create() still returns a valid object |
| TestAvailableTypesContainsBuiltIns  | AvailableTypes() contains "Serial", "TCP", "UDP", and "CAN"                      |
| TestAvailableTypesContainsRegisteredPlugin | A string-registered type appears in AvailableTypes()                       |

(Note: TestRegisterDuplicateKey and TestAvailableTypesContainsRegisteredPlugin are
counted within the 11 methods listed above but not all 11 are shown in this table;
the table covers all methods present in the source.)

### TestProtocolFactory

| Method                          | What is verified                                                                   |
|---------------------------------|------------------------------------------------------------------------------------|
| TestCreateModbusRTU             | Create("Modbus", {mode:RTU}) returns a non-null object with GetName() == "Modbus" |
| TestCreateModbusTCP             | Create("Modbus", {mode:TCP}) returns a non-null object                             |
| TestCreateCustomBinary          | Create("Custom", {subType:binary}) returns a non-null object                       |
| TestCreateCustomJson            | Create("Custom", {subType:json}) returns a non-null object                         |
| TestCreateFrame                 | Create("Frame", config) returns a non-null object with GetName() == "Frame"        |
| TestCreateUnknown               | Create("AlienProto_xyz") returns nullptr without crashing                          |
| TestAvailableTypesBuiltIns      | AvailableTypes() contains "Modbus", "Custom", and "Frame"                          |
| TestRegisterCustomProtocol      | RegisterProtocol("EchoFrame_test", fn) followed by Create("EchoFrame_test") returns a valid object |
| TestAvailableTypesAfterRegistration | A string-registered protocol appears in AvailableTypes()                       |

### TestCommunicationManager

All tests use built-in factory types; no actual serial port or network
connection is made.  Tests verify the state machine and return-value contracts.

| Method                            | What is verified                                                               |
|-----------------------------------|--------------------------------------------------------------------------------|
| TestInitializeSerial              | Initialize("Serial","Custom", serial_config) returns true and state is Idle    |
| TestInitializeTcp                 | Initialize("TCP","Modbus", tcp_config) returns true                            |
| TestInitializeFrame               | Initialize("UDP","Frame", udp_config, frame_config) returns true               |
| TestInitializeBadTransportType    | Initialize with an unregistered transport type returns false                   |
| TestInitializeBadProtocolType     | Initialize with an unregistered protocol type returns false                    |
| TestReInitialize                  | A second Initialize() call tears down the first session and succeeds           |
| TestIsConnectedInitiallyFalse     | IsConnected() returns false immediately after Initialize()                     |
| TestGetStateInitiallyIdle         | GetState() returns Idle after Initialize()                                     |
| TestSendMessageWhenNotConnected   | SendMessage() returns false when the transport is not connected                |
| TestSendRawDataWhenNotConnected   | SendRawData() returns false when the transport is not connected                |
| TestSendMessageBeforeInitialize   | SendMessage() returns false even before Initialize() is called                 |
| TestResetAfterInitialize          | Reset() after Initialize() sets state back to Idle without crashing            |
| TestResetWithoutInitializeIsHarmless | Reset() on a freshly constructed manager does not crash                     |
| TestGetTransportNullBeforeInit    | GetTransport() returns nullptr before Initialize()                             |
| TestGetProtocolNullBeforeInit     | GetProtocol() returns nullptr before Initialize()                              |
| TestGetTransportNonNullAfterInit  | GetTransport() returns a non-null pointer after Initialize()                   |
| TestGetProtocolNonNullAfterInit   | GetProtocol() returns a non-null pointer after Initialize()                    |

### TestChannelManager

| Method                                    | What is verified                                                              |
|-------------------------------------------|-------------------------------------------------------------------------------|
| TestAddChannelReturnsTrue                 | AddChannel() with valid types returns true                                    |
| TestAddChannelAppearsInIds                | ChannelIds() contains the new channel id after AddChannel()                   |
| TestAddChannelHasChannel                  | HasChannel() returns true for an added channel                                |
| TestAddChannelIncrementsCount             | Adding two channels makes ChannelCount() == 2                                 |
| TestAddChannelWithBadTransportFails       | AddChannel() with an unregistered transport returns false and does not increment the count |
| TestAddChannelWithBadProtocolFails        | AddChannel() with an unregistered protocol returns false                      |
| TestAddDuplicateChannelReturnsFalse       | Adding a channel with an already-used id returns false and count stays at 1   |
| TestRemoveChannelDecreasesCount           | RemoveChannel() decrements the count to 0                                     |
| TestRemoveChannelHasChannelReturnsFalse   | HasChannel() returns false after removal                                      |
| TestRemoveChannelGetChannelReturnsNull    | GetChannel() returns nullptr after removal                                    |
| TestRemoveUnknownChannelIsHarmless        | Calling RemoveChannel() with an unknown id does not crash                     |
| TestRemoveAllChannels                     | RemoveAllChannels() makes ChannelCount() == 0 and ChannelIds() empty          |
| TestGetChannelReturnsNonNull              | GetChannel() returns a non-null CommunicationManager* for a known id         |
| TestGetChannelUnknownReturnsNull          | GetChannel() returns nullptr for an unknown id                                |
| TestUpdateChannelReturnsTrueForExistingId | UpdateChannel() returns true and keeps the count at 1                         |
| TestUpdateChannelReturnsFalseForUnknownId | UpdateChannel() returns false for an id that was never added                  |
| TestUpdateChannelPreservesChannelId       | After UpdateChannel(), HasChannel() still returns true for the original id    |
| TestChannelAddedSignal                    | ChannelAdded(id) is emitted once with the correct channel id                  |
| TestChannelRemovedSignal                  | ChannelRemoved(id) is emitted once with the correct channel id                |
| TestChannelAddedSignalNotFiredOnDuplicate | ChannelAdded is not emitted when AddChannel() fails due to duplicate id       |

### TestIProtocolMessage

Tests `IProtocol::Message` as a plain data type.

| Method               | What is verified                                                                            |
|----------------------|---------------------------------------------------------------------------------------------|
| TestDefaultValues    | Default-constructed Message has is_valid == false, timestamp_ms == 0, empty payload, raw_data, and message_type |
| TestCopyIsValueSemantics | Copy-constructing a Message produces an independent copy; mutating the copy does not affect the original |
| TestMoveSemantics    | Move-constructing a Message transfers the raw_data content correctly                        |


---


## 8. Shared Test Utilities

### FrameParserTestHelpers (in tst_frame_parser.h)

```
quint16 RefCRC16IBM(const QByteArray& data, int begin, int end)
```
Independent CRC-16/IBM reference implementation used to verify
FrameParser::ComputeChecksum without trusting the implementation under test.

```
QByteArray MakeAA55Frame(quint8 addr, quint8 cmd, const QByteArray& data)
```
Builds a complete AA55 / addr / cmd / len-BE(2) / data / CRC16IBM-BE(2) frame.
Used in nearly every FrameParser and FrameProtocol test.

```
FrameSpec MakeSpec()
```
Returns the canonical test spec (AA55 header, addr + cmd named fields,
2-byte big-endian length, CRC16-IBM checksum).

### FrameProtocolTestHelpers (in tst_frame_protocol.h)

```
QByteArray MakeAA55Frame(quint8, quint8, const QByteArray&)
FrameSpec  MakeSpec()
QVariantMap MakeConfigMap()
```
Same frame builder and spec factory as above, plus a `MakeConfigMap()` that
produces the equivalent QVariantMap for testing `FromConfig()`.

### MockTransport (in tst_factories.h)

Described in section 7 above.


---


## 9. Adding New Tests

To add a test for a new class or feature:

1. Create a new header `tests/tst_<name>.h`.

2. Define a class that inherits `QObject` with `Q_OBJECT` and declares its
   test methods as `private slots`:

   ```cpp
   class TestMyFeature : public QObject
   {
       Q_OBJECT
   private slots:
       void TestBasicCase();
       void TestEdgeCase();
   };
   ```

3. Include the header in `main_tests.cpp` and add a `RunSuite<TestMyFeature>`
   call.

4. Add the header to the `HEADERS` section in `tests.pro`.

5. Add `#include "tst_<name>.moc"` at the bottom of `main_tests.cpp`.

Guidelines:

- One test method tests exactly one observable behaviour.  The method name
  must describe the behaviour being verified ("TestSendMessageWhenNotConnected"
  not "TestSend2").
- Use `QCOMPARE` for equality checks.  Use `QVERIFY` only when there is no
  meaningful expected value to compare against.
- Use `QSignalSpy` to verify that signals are emitted with the correct arguments
  rather than connecting lambda slots in test methods.
- When a test exercises a code path that requires a physical device (serial port,
  CAN interface), guard it with `QSKIP` and a descriptive message so CI remains
  green without hardware present:

  ```cpp
  void TestConnectRealSerial()
  {
      if (!QSerialPortInfo::availablePorts().isEmpty())
          QSKIP("No serial port available on this machine");
      // ...
  }
  ```
