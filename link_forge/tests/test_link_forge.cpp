/**
 * LinkForge Unit Tests
 *
 * Build: Add this file to a separate Qt Test executable target.
 *        Link against: Qt::Test, Qt::Core, Qt::SerialBus
 *        and the LinkForge static/object libraries.
 *
 * Run all suites:
 *   ./link_forge_tests -v2
 *
 * Run a single suite:
 *   ./link_forge_tests TestFrameParser -v2
 */

#include <QtTest/QtTest>
#include <QVariantMap>
#include <QByteArray>
#include <QSignalSpy>
#include <optional>
#include <memory>

#include "core/itransport.h"
#include "core/iprotocol.h"
#include "core/communication_manager.h"
#include "core/channel_manager.h"
#include "core/transport_factory.h"
#include "core/protocol_factory.h"
#include "protocol/frame_parser.h"
#include "protocol/frame_protocol.h"
#include "protocol/custom_protocol.h"

using namespace LinkForge::Core;

// ============================================================
//  Helpers
// ============================================================

namespace {

/**
 * Build an AA55 binary frame identical to what CustomProtocol produces:
 *   [0xAA][0x55][type 1B][len 2B big-endian][payload][checksum 1B = sum of payload bytes]
 */
QByteArray MakeCustomFrame(quint8 type, const QByteArray& payload)
{
    QByteArray frame;
    frame.append(static_cast<char>(0xAA));
    frame.append(static_cast<char>(0x55));
    frame.append(static_cast<char>(type));
    quint16 len = static_cast<quint16>(payload.size());
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(payload);

    quint8 sum = 0;
    for (char c : payload) sum += static_cast<quint8>(c);
    frame.append(static_cast<char>(sum));
    return frame;
}

/**
 * Compute CRC-16/IBM (Modbus poly 0x8005, init 0xFFFF) over a byte range.
 */
quint16 CRC16_IBM(const QByteArray& data, int begin = 0, int end = -1)
{
    int last = (end < 0) ? data.size() : end;
    quint16 crc = 0xFFFF;
    for (int i = begin; i < last; ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            else         crc >>= 1;
        }
    }
    return crc;
}

/**
 * Build a frame conforming to:
 *   AA55 | addr(1) | cmd(1) | len(2 big-endian) | data | CRC16_IBM(2 big-endian)
 */
QByteArray MakeFrameParserFrame(quint8 addr, quint8 cmd, const QByteArray& data)
{
    QByteArray frame;
    frame.append(static_cast<char>(0xAA));
    frame.append(static_cast<char>(0x55));
    frame.append(static_cast<char>(addr));
    frame.append(static_cast<char>(cmd));
    quint16 len = static_cast<quint16>(data.size());
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(data);

    // CRC over everything before the checksum field
    quint16 crc = CRC16_IBM(frame);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));
    return frame;
}

/**
 * Build the canonical FrameSpec used by the FrameParser tests:
 *   AA55 | addr(1B) | cmd(1B) | len(2B big-endian) | data | CRC16_IBM(2B big-endian)
 */
FrameSpec MakeTestSpec()
{
    FrameSpec spec;
    spec.header_magic       = QByteArray::fromHex("AA55");
    spec.named_fields       = {
        {"addr", 2, 1, true},   // offset 2, size 1
        {"cmd",  3, 1, true},   // offset 3, size 1
    };
    spec.length_field_offset = 4;
    spec.length_field_size   = 2;
    spec.length_big_endian   = true;
    spec.length_adjustment   = 0;
    spec.data_offset         = 6;
    spec.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::CRC16_IBM;
    spec.checksum_size       = 2;
    spec.checksum_big_endian = true;
    return spec;
}

// ============================================================
//  MockTransport – minimal in-process transport for unit tests
// ============================================================

class MockTransport : public ITransport
{
    Q_OBJECT
public:
    explicit MockTransport(QObject* parent = nullptr)
        : ITransport(parent) {}

    // --- ITransport interface ---

    bool Connect(const QVariantMap& cfg) override
    {
        config_ = cfg;
        state_  = State::Connected;
        emit Connected();
        emit StateChanged(state_);
        return true;
    }

    void Disconnect() override
    {
        state_ = State::Disconnected;
        emit Disconnected();
        emit StateChanged(state_);
    }

    qint64 Send(const QByteArray& data) override
    {
        last_sent_ = data;
        sent_count_++;
        return data.size();
    }

    State        GetState()  const override { return state_; }
    Type         GetType()   const override { return Type::Unknown; }
    QString      GetName()   const override { return "Mock"; }
    QVariantMap  GetConfig() const override { return config_; }

    // Test helpers
    void SimulateReceive(const QByteArray& data) { emit DataReceived(data); }
    void SimulateError(const QString& msg)       { emit ErrorOccurred(msg); }

    QByteArray LastSent()  const { return last_sent_; }
    int        SentCount() const { return sent_count_; }
    void       ResetCounters()   { last_sent_.clear(); sent_count_ = 0; }

private:
    State       state_      {State::Disconnected};
    QVariantMap config_;
    QByteArray  last_sent_;
    int         sent_count_ {0};
};

} // anonymous namespace

// ============================================================
//  TestFrameParser
// ============================================================

class TestFrameParser : public QObject
{
    Q_OBJECT

private slots:

    // Single complete frame, all fields present
    void TestFeedSingleFrame()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        QByteArray payload = QByteArray::fromHex("01020304");
        QByteArray frame   = MakeFrameParserFrame(0x01, 0x02, payload);

        auto results = parser.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QVERIFY(results[0].is_valid);
        QCOMPARE(results[0].raw_data, frame);
        QCOMPARE(results[0].payload.value("addr").toUInt(), 0x01u);
        QCOMPARE(results[0].payload.value("cmd").toUInt(),  0x02u);
        QCOMPARE(results[0].payload.value("data").toByteArray(), payload);
    }

    // Feed the same complete frame in two separate partial calls
    void TestFeedPartialFrame()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        QByteArray payload = QByteArray::fromHex("AABBCCDD");
        QByteArray frame   = MakeFrameParserFrame(0x10, 0x20, payload);

        // Split at an arbitrary byte mid-frame
        int split = frame.size() / 2;
        auto r1 = parser.Feed(frame.left(split));
        QVERIFY(r1.empty());  // not enough data yet

        auto r2 = parser.Feed(frame.mid(split));
        QCOMPARE(r2.size(), std::size_t(1));
        QVERIFY(r2[0].is_valid);
    }

    // Two back-to-back frames in a single Feed() call
    void TestFeedTwoFrames()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        QByteArray f1 = MakeFrameParserFrame(0x01, 0x01, QByteArray("hello"));
        QByteArray f2 = MakeFrameParserFrame(0x02, 0x02, QByteArray("world!"));

        auto results = parser.Feed(f1 + f2);
        QCOMPARE(results.size(), std::size_t(2));
        QVERIFY(results[0].is_valid);
        QVERIFY(results[1].is_valid);
        QCOMPARE(results[0].payload.value("addr").toUInt(), 0x01u);
        QCOMPARE(results[1].payload.value("addr").toUInt(), 0x02u);
    }

    // Wrong CRC must produce is_valid == false
    void TestInvalidChecksum()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        QByteArray frame = MakeFrameParserFrame(0x01, 0x01, QByteArray("data"));
        // Flip last byte (CRC low byte)
        frame[frame.size() - 1] = frame[frame.size() - 1] ^ 0xFF;

        auto results = parser.Feed(frame);
        // Parser may either reject (empty result) or return is_valid=false
        if (!results.empty()) {
            QVERIFY(!results[0].is_valid);
        }
    }

    // Reset() must flush the internal reassembly buffer
    void TestReset()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        QByteArray payload = QByteArray("test");
        QByteArray frame   = MakeFrameParserFrame(0x01, 0x01, payload);
        int split = frame.size() / 2;

        parser.Feed(frame.left(split));   // partial — buffered
        parser.Reset();                   // flush

        // Now feed the rest: without the first half, it should not produce a valid frame
        auto results = parser.Feed(frame.mid(split));
        QVERIFY(results.empty());
    }

    // Named fields with specific sizes
    void TestNamedFieldsDecoded()
    {
        // Frame: AA55 | uint16-BE value | len(2) | data | no checksum
        FrameSpec spec;
        spec.header_magic        = QByteArray::fromHex("AA55");
        spec.named_fields        = {
            {"voltage", 2, 2, true},  // 2-byte big-endian at offset 2
        };
        spec.length_field_offset = 4;
        spec.length_field_size   = 2;
        spec.data_offset         = 6;
        spec.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::None;

        FrameParser parser(spec);

        QByteArray frame;
        frame.append(QByteArray::fromHex("AA55")); // header
        frame.append(static_cast<char>(0x13));     // voltage high byte = 0x13
        frame.append(static_cast<char>(0x88));     // voltage low  byte = 0x88 → 5000
        frame.append(static_cast<char>(0x00));     // length high
        frame.append(static_cast<char>(0x02));     // length low = 2
        frame.append(static_cast<char>(0xAA));     // data[0]
        frame.append(static_cast<char>(0xBB));     // data[1]

        auto results = parser.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QCOMPARE(results[0].payload.value("voltage").toUInt(), 0x1388u);  // 5000
    }

    // SetSpec() replaces the spec and Reset() is implicit
    void TestSetSpec()
    {
        FrameSpec spec = MakeTestSpec();
        FrameParser parser(spec);

        FrameSpec spec2;
        spec2.header_magic       = QByteArray::fromHex("FFEE");
        spec2.length_field_offset = 2;
        spec2.length_field_size  = 1;
        spec2.data_offset        = 3;
        spec2.checksum_algorithm = FrameSpec::ChecksumAlgorithm::None;

        parser.SetSpec(spec2);
        QCOMPARE(parser.GetSpec().header_magic, spec2.header_magic);
    }

    // Static checksum helper
    void TestComputeChecksumCRC16IBM()
    {
        // Modbus RTU known test vector: 0x01 0x03 0x00 0x00 0x00 0x02 → CRC = 0xC40B
        QByteArray data = QByteArray::fromHex("010300000002");
        quint32 computed = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC16_IBM, data);
        QCOMPARE(computed, quint32(0xC40B));
    }

    void TestComputeChecksumSum8()
    {
        QByteArray data = QByteArray::fromHex("01020304");
        quint32 computed = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::Sum8, data);
        QCOMPARE(computed, quint32((0x01 + 0x02 + 0x03 + 0x04) & 0xFF));
    }
};

// ============================================================
//  TestFrameProtocol
// ============================================================

class TestFrameProtocol : public QObject
{
    Q_OBJECT

private slots:

    // BuildMessage then ParseData round-trip
    void TestBuildAndParse()
    {
        FrameSpec spec = MakeTestSpec();
        FrameProtocol proto(spec);

        // Only the data payload is variable; addr/cmd are in named_fields (fixed offset)
        QVariantMap payload;
        payload["data"] = QByteArray::fromHex("DEADBEEF");

        auto encoded = proto.BuildMessage(payload);
        QVERIFY(encoded.has_value());
        QVERIFY(!encoded->isEmpty());

        auto messages = proto.ParseData(*encoded);
        QCOMPARE(messages.size(), std::size_t(1));
        QVERIFY(messages[0].is_valid);
        QCOMPARE(messages[0].payload.value("data").toByteArray(),
                 QByteArray::fromHex("DEADBEEF"));
    }

    // FromConfig must construct a usable FrameProtocol
    void TestFromConfig()
    {
        QVariantMap cfg;
        cfg["headerMagic"]       = "AA55";
        cfg["lengthFieldOffset"] = 4;
        cfg["lengthFieldSize"]   = 2;
        cfg["dataOffset"]        = 6;
        cfg["checksumAlgorithm"] = "crc16ibm";
        cfg["checksumSize"]      = 2;

        auto proto = FrameProtocol::FromConfig(cfg);
        QVERIFY(proto != nullptr);
        QCOMPARE(proto->GetSpec().header_magic, QByteArray::fromHex("AA55"));
        QCOMPARE(proto->GetSpec().checksum_algorithm,
                 FrameSpec::ChecksumAlgorithm::CRC16_IBM);
    }

    // ValidateMessage must return false for a frame with wrong magic
    void TestValidateWrongMagic()
    {
        FrameSpec spec = MakeTestSpec();
        FrameProtocol proto(spec);

        QByteArray bad = MakeFrameParserFrame(0x01, 0x01, QByteArray("x"));
        bad[0] = 0x00;  // corrupt header
        QVERIFY(!proto.ValidateMessage(bad));
    }

    // MessageParsed signal is emitted for each valid frame
    void TestSignalEmitted()
    {
        FrameSpec spec = MakeTestSpec();
        FrameProtocol proto(spec);

        QSignalSpy spy(&proto, &IProtocol::MessageParsed);

        QByteArray frame = MakeFrameParserFrame(0x01, 0x01, QByteArray("hello"));
        proto.ParseData(frame);

        QCOMPARE(spy.count(), 1);
        auto msg = qvariant_cast<IProtocol::Message>(spy.at(0).at(0));
        QVERIFY(msg.is_valid);
    }

    // Reset() clears mid-frame buffer
    void TestResetClearsBuffer()
    {
        FrameSpec spec = MakeTestSpec();
        FrameProtocol proto(spec);

        QByteArray frame = MakeFrameParserFrame(0x01, 0x01, QByteArray("test"));
        int split = frame.size() / 2;

        proto.ParseData(frame.left(split));
        proto.Reset();
        auto results = proto.ParseData(frame.mid(split));
        QVERIFY(results.empty());
    }
};

// ============================================================
//  TestTransportFactory
// ============================================================

class TestTransportFactory : public QObject
{
    Q_OBJECT

private slots:

    // Create by enum type
    void TestCreateSerial()
    {
        auto t = TransportFactory::Create(ITransport::Type::Serial);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::Serial);
    }

    void TestCreateTcp()
    {
        auto t = TransportFactory::Create(ITransport::Type::TcpSocket);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::TcpSocket);
    }

    void TestCreateUdp()
    {
        auto t = TransportFactory::Create(ITransport::Type::UdpSocket);
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetType(), ITransport::Type::UdpSocket);
    }

    // Create by string name
    void TestCreateByStringSerial()
    {
        auto t = TransportFactory::Create("Serial");
        QVERIFY(t != nullptr);
    }

    void TestCreateByStringTcp()
    {
        auto t = TransportFactory::Create("TCP");
        QVERIFY(t != nullptr);
    }

    // Unknown name must return nullptr (no crash)
    void TestCreateUnknown()
    {
        auto t = TransportFactory::Create("NonExistentTransport");
        QVERIFY(t == nullptr);
    }

    // Register a custom string-keyed type and retrieve it
    void TestRegisterAndCreateCustom()
    {
        TransportFactory::RegisterTransport("MockBus",
            [](QObject* parent) -> ITransport::Ptr {
                return std::make_shared<MockTransport>(parent);
            });

        auto t = TransportFactory::Create("MockBus");
        QVERIFY(t != nullptr);
        QCOMPARE(t->GetName(), QString("Mock"));
    }

    // AvailableTypes must list at least the 4 built-ins
    void TestAvailableTypes()
    {
        QStringList types = TransportFactory::AvailableTypes();
        QVERIFY(types.contains("Serial"));
        QVERIFY(types.contains("TCP"));
        QVERIFY(types.contains("UDP"));
        QVERIFY(types.contains("CAN"));
    }
};

// ============================================================
//  TestProtocolFactory
// ============================================================

class TestProtocolFactory : public QObject
{
    Q_OBJECT

private slots:

    void TestCreateModbus()
    {
        auto p = ProtocolFactory::Create("Modbus", {{"mode","RTU"}});
        QVERIFY(p != nullptr);
    }

    void TestCreateCustom()
    {
        auto p = ProtocolFactory::Create("Custom");
        QVERIFY(p != nullptr);
    }

    void TestCreateFrame()
    {
        QVariantMap cfg;
        cfg["headerMagic"]       = "AA55";
        cfg["lengthFieldOffset"] = 4;
        cfg["lengthFieldSize"]   = 2;
        cfg["dataOffset"]        = 6;
        cfg["checksumAlgorithm"] = "crc16ibm";

        auto p = ProtocolFactory::Create("Frame", cfg);
        QVERIFY(p != nullptr);
    }

    void TestCreateUnknown()
    {
        auto p = ProtocolFactory::Create("NoSuchProtocol");
        QVERIFY(p == nullptr);
    }

    void TestAvailableTypes()
    {
        QStringList types = ProtocolFactory::AvailableTypes();
        QVERIFY(types.contains("Modbus"));
        QVERIFY(types.contains("Custom"));
        QVERIFY(types.contains("Frame"));
    }

    // Register a custom protocol type
    void TestRegisterCustomProtocol()
    {
        ProtocolFactory::RegisterProtocol("EchoProto",
            [](const QVariantMap& cfg, QObject* parent) -> IProtocol::Ptr {
                // Re-use FrameProtocol as a stand-in
                return FrameProtocol::FromConfig(cfg, parent);
            });

        QVariantMap cfg;
        cfg["headerMagic"]       = "AA55";
        cfg["lengthFieldOffset"] = 2;
        cfg["lengthFieldSize"]   = 2;
        cfg["dataOffset"]        = 4;
        cfg["checksumAlgorithm"] = "none";

        auto p = ProtocolFactory::Create("EchoProto", cfg);
        QVERIFY(p != nullptr);
    }
};

// ============================================================
//  TestCustomProtocol
// ============================================================

class TestCustomProtocol : public QObject
{
    Q_OBJECT

private slots:

    // Binary AA55 round-trip
    void TestBuildAndParseAA55()
    {
        auto proto = ProtocolFactory::Create("Custom", {{"subType","binary"}});
        QVERIFY(proto);

        QVariantMap payload;
        payload["type"]    = 0x01;
        payload["message"] = QString("hello");

        auto encoded = proto->BuildMessage(payload);
        QVERIFY(encoded.has_value());

        auto messages = proto->ParseData(*encoded);
        QVERIFY(!messages.empty());
        QVERIFY(messages[0].is_valid);
    }

    // JSON newline-delimited round-trip
    void TestJsonProtocol()
    {
        auto proto = ProtocolFactory::Create("Custom", {{"subType","json"}});
        QVERIFY(proto);

        QVariantMap payload;
        payload["key"]   = "value";
        payload["count"] = 42;

        auto encoded = proto->BuildMessage(payload);
        QVERIFY(encoded.has_value());
        // Must end with newline
        QCOMPARE(encoded->right(1), QByteArray("\n"));

        auto messages = proto->ParseData(*encoded);
        QVERIFY(!messages.empty());
        QVERIFY(messages[0].is_valid);
        QCOMPARE(messages[0].payload.value("key").toString(), QString("value"));
        QCOMPARE(messages[0].payload.value("count").toInt(), 42);
    }

    // Feed partial JSON line, then the rest
    void TestJsonPartialLine()
    {
        auto proto = ProtocolFactory::Create("Custom", {{"subType","json"}});
        QVERIFY(proto);

        QByteArray line = QByteArray("{\"x\":1}\n");
        auto r1 = proto->ParseData(line.left(4));
        QVERIFY(r1.empty());  // no complete line yet

        auto r2 = proto->ParseData(line.mid(4));
        QCOMPARE(r2.size(), std::size_t(1));
        QVERIFY(r2[0].is_valid);
    }

    // ValidateMessage should recognize a well-formed AA55 frame
    void TestValidateBinaryFrame()
    {
        auto proto = ProtocolFactory::Create("Custom", {{"subType","binary"}});
        QVERIFY(proto);

        QByteArray frame = MakeCustomFrame(0x01, QByteArray("payload"));
        QVERIFY(proto->ValidateMessage(frame));
    }

    // Corrupted frame must not validate
    void TestValidateCorruptedBinaryFrame()
    {
        auto proto = ProtocolFactory::Create("Custom", {{"subType","binary"}});
        QVERIFY(proto);

        QByteArray frame = MakeCustomFrame(0x01, QByteArray("payload"));
        frame[frame.size() - 1] ^= 0xFF;  // flip checksum
        QVERIFY(!proto->ValidateMessage(frame));
    }
};

// ============================================================
//  TestCommunicationManager
// ============================================================

class TestCommunicationManager : public QObject
{
    Q_OBJECT

private slots:

    // Initialize with valid type strings must succeed
    void TestInitialize()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ok);
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    // Unknown transport type must fail cleanly
    void TestInitializeBadTransport()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("GhostBus", "Custom", {});
        QVERIFY(!ok);
    }

    // Unknown protocol type must fail cleanly
    void TestInitializeBadProtocol()
    {
        CommunicationManager mgr;
        bool ok = mgr.Initialize("Serial", "AlienProto", {{"port","COM1"}});
        QVERIFY(!ok);
    }

    // SendMessage must return false when not connected
    void TestSendWhenNotConnected()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});

        QSignalSpy spy(&mgr, &CommunicationManager::ErrorOccurred);
        bool ok = mgr.SendMessage({{"data","test"}});
        QVERIFY(!ok);
    }

    // SendRawData must return false when not connected
    void TestSendRawWhenNotConnected()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        bool ok = mgr.SendRawData(QByteArray("bytes"));
        QVERIFY(!ok);
    }

    // IsConnected must reflect the manager state
    void TestIsConnected()
    {
        CommunicationManager mgr;
        mgr.Initialize("Serial", "Custom", {{"port","COM1"}});
        QVERIFY(!mgr.IsConnected());
    }

    // Re-initializing an already-initialized manager must work (teardown + re-init)
    void TestReInitialize()
    {
        CommunicationManager mgr;
        QVERIFY(mgr.Initialize("Serial", "Custom", {{"port","COM1"}}));
        QVERIFY(mgr.Initialize("TCP",    "Custom", {{"host","127.0.0.1"},{"port",502}}));
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }

    // Reset() after failed init is safe
    void TestResetAfterFailedInit()
    {
        CommunicationManager mgr;
        mgr.Initialize("NoSuchBus", "Custom", {});
        // Must not crash
        mgr.Reset();
        QCOMPARE(mgr.GetState(), CommunicationManager::State::Idle);
    }
};

// ============================================================
//  TestChannelManager
// ============================================================

class TestChannelManager : public QObject
{
    Q_OBJECT

private slots:

    // AddChannel returns true and ChannelIds contains the new id
    void TestAddChannel()
    {
        ChannelManager ch;
        bool ok = ch.AddChannel("ch1", "Serial", "Custom",
                                {{"port","COM1"}});
        QVERIFY(ok);
        QVERIFY(ch.HasChannel("ch1"));
        QVERIFY(ch.ChannelIds().contains("ch1"));
        QCOMPARE(ch.ChannelCount(), 1);
    }

    // AddChannel with a duplicate id must return false
    void TestDuplicateChannel()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        bool ok = ch.AddChannel("ch1", "TCP", "Custom",
                                {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(!ok);
        QCOMPARE(ch.ChannelCount(), 1);
    }

    // RemoveChannel must remove the channel
    void TestRemoveChannel()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        ch.RemoveChannel("ch1");
        QVERIFY(!ch.HasChannel("ch1"));
        QCOMPARE(ch.ChannelCount(), 0);
    }

    // RemoveAllChannels clears everything
    void TestRemoveAllChannels()
    {
        ChannelManager ch;
        ch.AddChannel("a", "Serial", "Custom", {{"port","COM1"}});
        ch.AddChannel("b", "TCP",    "Custom", {{"host","127.0.0.1"},{"port",502}});
        ch.RemoveAllChannels();
        QCOMPARE(ch.ChannelCount(), 0);
    }

    // GetChannel returns a non-null pointer for an existing channel
    void TestGetChannel()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QVERIFY(ch.GetChannel("ch1") != nullptr);
    }

    // GetChannel returns nullptr for an unknown id
    void TestGetChannelUnknown()
    {
        ChannelManager ch;
        QVERIFY(ch.GetChannel("does_not_exist") == nullptr);
    }

    // ChannelAdded signal is emitted
    void TestChannelAddedSignal()
    {
        ChannelManager ch;
        QSignalSpy spy(&ch, &ChannelManager::ChannelAdded);

        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("ch1"));
    }

    // ChannelRemoved signal is emitted
    void TestChannelRemovedSignal()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});
        QSignalSpy spy(&ch, &ChannelManager::ChannelRemoved);

        ch.RemoveChannel("ch1");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("ch1"));
    }

    // Adding multiple channels with different types
    void TestMultipleChannels()
    {
        ChannelManager ch;
        ch.AddChannel("serial1", "Serial", "Modbus",
                      {{"port","COM1"},{"baudRate",115200}},
                      {{"mode","RTU"}});
        ch.AddChannel("tcp1", "TCP", "Modbus",
                      {{"host","127.0.0.1"},{"port",502}},
                      {{"mode","TCP"}});
        ch.AddChannel("udp1", "UDP", "Custom",
                      {{"remoteHost","192.168.1.1"},{"remotePort",4000}});

        QCOMPARE(ch.ChannelCount(), 3);
        QVERIFY(ch.HasChannel("serial1"));
        QVERIFY(ch.HasChannel("tcp1"));
        QVERIFY(ch.HasChannel("udp1"));
    }

    // UpdateChannel replaces an existing channel
    void TestUpdateChannel()
    {
        ChannelManager ch;
        ch.AddChannel("ch1", "Serial", "Custom", {{"port","COM1"}});

        bool ok = ch.UpdateChannel("ch1", "TCP", "Custom",
                                   {{"host","127.0.0.1"},{"port",502}});
        QVERIFY(ok);
        QCOMPARE(ch.ChannelCount(), 1);
        QVERIFY(ch.HasChannel("ch1"));
    }

    // UpdateChannel on unknown id must return false
    void TestUpdateChannelUnknown()
    {
        ChannelManager ch;
        bool ok = ch.UpdateChannel("nobody", "TCP", "Custom", {});
        QVERIFY(!ok);
    }
};

// ============================================================
//  TestIProtocolMessage
// ============================================================

class TestIProtocolMessage : public QObject
{
    Q_OBJECT

private slots:

    // Default-constructed Message has sensible defaults
    void TestDefaultMessage()
    {
        IProtocol::Message msg;
        QVERIFY(!msg.is_valid);
        QCOMPARE(msg.timestamp_ms, qint64(0));
        QVERIFY(msg.payload.isEmpty());
        QVERIFY(msg.raw_data.isEmpty());
        QVERIFY(msg.message_type.isEmpty());
    }

    // Message is copyable (value type)
    void TestMessageCopy()
    {
        IProtocol::Message a;
        a.is_valid      = true;
        a.timestamp_ms  = 12345;
        a.message_type  = "TestType";
        a.raw_data      = QByteArray("rawbytes");
        a.payload["k"]  = "v";

        IProtocol::Message b = a;
        QCOMPARE(b.is_valid,     a.is_valid);
        QCOMPARE(b.timestamp_ms, a.timestamp_ms);
        QCOMPARE(b.message_type, a.message_type);
        QCOMPARE(b.raw_data,     a.raw_data);
        QCOMPARE(b.payload,      a.payload);
    }
};

// ============================================================
//  Entry point
// ============================================================

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    int failures = 0;

    {
        TestFrameParser t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestFrameProtocol t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestTransportFactory t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestProtocolFactory t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestCustomProtocol t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestCommunicationManager t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestChannelManager t;
        failures += QTest::qExec(&t, argc, argv);
    }
    {
        TestIProtocolMessage t;
        failures += QTest::qExec(&t, argc, argv);
    }

    return failures;
}

#include "test_link_forge.moc"
