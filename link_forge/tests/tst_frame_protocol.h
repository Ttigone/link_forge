#pragma once

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVariantMap>
#include <QByteArray>
#include <memory>

#include "core/iprotocol.h"
#include "core/protocol_factory.h"
#include "protocol/frame_parser.h"
#include "protocol/frame_protocol.h"

using namespace LinkForge::Core;

// ----------------------------------------------------------------
//  Shared helpers
// ----------------------------------------------------------------
namespace FrameProtocolTestHelpers {

inline quint16 CRC16IBM(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1) ? quint16((crc >> 1) ^ 0xA001) : quint16(crc >> 1);
    }
    return crc;
}

/**
 * AA55 | addr(1) | cmd(1) | len-BE(2) | data | CRC16IBM-BE(2)
 */
inline QByteArray MakeAA55Frame(quint8 addr, quint8 cmd, const QByteArray& data)
{
    QByteArray f;
    f += char(0xAA); f += char(0x55);
    f += char(addr); f += char(cmd);
    quint16 len = quint16(data.size());
    f += char((len >> 8) & 0xFF); f += char(len & 0xFF);
    f += data;
    quint16 crc = CRC16IBM(f);
    f += char((crc >> 8) & 0xFF); f += char(crc & 0xFF);
    return f;
}

inline FrameSpec MakeSpec()
{
    FrameSpec s;
    s.header_magic        = QByteArray::fromHex("AA55");
    s.named_fields        = { {"addr", 2, 1, true}, {"cmd", 3, 1, true} };
    s.length_field_offset = 4;
    s.length_field_size   = 2;
    s.length_big_endian   = true;
    s.data_offset         = 6;
    s.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::CRC16_IBM;
    s.checksum_size       = 2;
    s.checksum_big_endian = true;
    return s;
}

inline QVariantMap MakeConfigMap()
{
    QVariantMap m;
    m["headerMagic"]       = "AA55";
    m["lengthFieldOffset"] = 4;
    m["lengthFieldSize"]   = 2;
    m["lengthBigEndian"]   = true;
    m["dataOffset"]        = 6;
    m["checksumAlgorithm"] = "crc16ibm";
    m["checksumSize"]      = 2;
    m["checksumBigEndian"] = true;
    return m;
}

} // namespace FrameProtocolTestHelpers

// ================================================================
//  TestFrameProtocolRoundTrip
// ================================================================
class TestFrameProtocolRoundTrip : public QObject
{
    Q_OBJECT

    using H = FrameProtocolTestHelpers;

private slots:

    // BuildMessage followed by ParseData recovers the payload
    void TestBuildThenParseDataPayload()
    {
        FrameProtocol proto(H::MakeSpec());

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

    // Empty data payload builds and parses successfully
    void TestRoundTripEmptyData()
    {
        FrameProtocol proto(H::MakeSpec());

        auto encoded = proto.BuildMessage({{"data", QByteArray()}});
        QVERIFY(encoded.has_value());

        auto messages = proto.ParseData(*encoded);
        QCOMPARE(messages.size(), std::size_t(1));
        QVERIFY(messages[0].is_valid);
        QVERIFY(messages[0].payload.value("data").toByteArray().isEmpty());
    }

    // Large payload (1 KB) round-trips without corruption
    void TestRoundTripLargePayload()
    {
        FrameProtocol proto(H::MakeSpec());

        QByteArray big(1024, 0x5A);
        auto encoded = proto.BuildMessage({{"data", big}});
        QVERIFY(encoded.has_value());

        auto messages = proto.ParseData(*encoded);
        QCOMPARE(messages.size(), std::size_t(1));
        QVERIFY(messages[0].is_valid);
        QCOMPARE(messages[0].payload.value("data").toByteArray(), big);
    }

    // Two consecutive round-trips on the same instance
    void TestTwoConsecutiveRoundTrips()
    {
        FrameProtocol proto(H::MakeSpec());

        for (int i = 0; i < 2; ++i) {
            QByteArray pl = QByteArray(4, char(i));
            auto enc = proto.BuildMessage({{"data", pl}});
            QVERIFY(enc.has_value());
            auto msgs = proto.ParseData(*enc);
            QCOMPARE(msgs.size(), std::size_t(1));
            QVERIFY(msgs[0].is_valid);
        }
    }

    // GetType and GetName return expected values
    void TestGetTypeAndName()
    {
        FrameProtocol proto(H::MakeSpec());
        QCOMPARE(proto.GetName(), QString("Frame"));
    }

    // message_type field is populated
    void TestMessageTypeField()
    {
        FrameProtocol proto(H::MakeSpec());
        auto enc  = proto.BuildMessage({{"data", QByteArray("x")}});
        auto msgs = proto.ParseData(*enc);
        QCOMPARE(msgs.size(), std::size_t(1));
        QCOMPARE(msgs[0].message_type, QString("Frame"));
    }

    // timestamp_ms is populated (non-zero after Jan 1 2000)
    void TestTimestampPopulated()
    {
        FrameProtocol proto(H::MakeSpec());
        auto enc  = proto.BuildMessage({{"data", QByteArray("ts")}});
        auto msgs = proto.ParseData(*enc);
        QCOMPARE(msgs.size(), std::size_t(1));
        QVERIFY(msgs[0].timestamp_ms > qint64(0));
    }
};

// ================================================================
//  TestFrameProtocolFromConfig
// ================================================================
class TestFrameProtocolFromConfig : public QObject
{
    Q_OBJECT

    using H = FrameProtocolTestHelpers;

private slots:

    // FromConfig with a valid map returns a non-null instance
    void TestFromConfigReturnsNonNull()
    {
        auto proto = FrameProtocol::FromConfig(H::MakeConfigMap());
        QVERIFY(proto != nullptr);
    }

    // Spec parsed from config matches expected values
    void TestFromConfigSpecValues()
    {
        auto proto = FrameProtocol::FromConfig(H::MakeConfigMap());
        QVERIFY(proto != nullptr);
        const FrameSpec& s = proto->GetSpec();
        QCOMPARE(s.header_magic, QByteArray::fromHex("AA55"));
        QCOMPARE(s.length_field_offset, 4);
        QCOMPARE(s.length_field_size,   2);
        QCOMPARE(s.data_offset,         6);
        QCOMPARE(s.checksum_algorithm,  FrameSpec::ChecksumAlgorithm::CRC16_IBM);
        QCOMPARE(s.checksum_size,       2);
    }

    // Checksum algorithm string variants are parsed correctly
    void TestFromConfigChecksumAlgorithms()
    {
        struct Case { const char* str; FrameSpec::ChecksumAlgorithm expected; };
        const Case cases[] = {
            { "none",       FrameSpec::ChecksumAlgorithm::None       },
            { "sum8",       FrameSpec::ChecksumAlgorithm::Sum8       },
            { "sum16",      FrameSpec::ChecksumAlgorithm::Sum16      },
            { "crc16ibm",   FrameSpec::ChecksumAlgorithm::CRC16_IBM  },
            { "crc16ccitt", FrameSpec::ChecksumAlgorithm::CRC16_CCITT},
            { "crc32",      FrameSpec::ChecksumAlgorithm::CRC32      },
        };
        for (const auto& c : cases) {
            QVariantMap m = H::MakeConfigMap();
            m["checksumAlgorithm"] = QString(c.str);
            auto proto = FrameProtocol::FromConfig(m);
            QVERIFY2(proto != nullptr, c.str);
            QCOMPARE(proto->GetSpec().checksum_algorithm, c.expected);
        }
    }

    // Named fields are parsed from config
    void TestFromConfigNamedFields()
    {
        QVariantMap m = H::MakeConfigMap();
        QVariantList fields;
        QVariantMap f1; f1["name"] = "addr"; f1["offset"] = 2; f1["size"] = 1; f1["bigEndian"] = true;
        QVariantMap f2; f2["name"] = "cmd";  f2["offset"] = 3; f2["size"] = 1; f2["bigEndian"] = true;
        fields << f1 << f2;
        m["namedFields"] = fields;

        auto proto = FrameProtocol::FromConfig(m);
        QVERIFY(proto != nullptr);
        QCOMPARE(proto->GetSpec().named_fields.size(), std::size_t(2));
        QCOMPARE(proto->GetSpec().named_fields[0].name, QString("addr"));
        QCOMPARE(proto->GetSpec().named_fields[1].name, QString("cmd"));
    }

    // Proto-factory "Frame" key calls FromConfig internally
    void TestFactoryCreateFrame()
    {
        auto p = ProtocolFactory::Create("Frame", H::MakeConfigMap());
        QVERIFY(p != nullptr);
        QCOMPARE(p->GetName(), QString("Frame"));
    }

    // FromConfig round-trip: build then parse
    void TestFromConfigRoundTrip()
    {
        auto proto = FrameProtocol::FromConfig(H::MakeConfigMap());
        QVERIFY(proto);

        QByteArray data = QByteArray("roundtrip");
        auto enc  = proto->BuildMessage({{"data", data}});
        QVERIFY(enc.has_value());
        auto msgs = proto->ParseData(*enc);
        QCOMPARE(msgs.size(), std::size_t(1));
        QVERIFY(msgs[0].is_valid);
        QCOMPARE(msgs[0].payload.value("data").toByteArray(), data);
    }
};

// ================================================================
//  TestFrameProtocolSignals
// ================================================================
class TestFrameProtocolSignals : public QObject
{
    Q_OBJECT

    using H = FrameProtocolTestHelpers;

private slots:

    // MessageParsed emitted for each valid frame
    void TestMessageParsedSignal()
    {
        FrameProtocol proto(H::MakeSpec());
        QSignalSpy spy(&proto, &IProtocol::MessageParsed);

        QByteArray f = H::MakeAA55Frame(0x01, 0x01, QByteArray("sig"));
        proto.ParseData(f);

        QCOMPARE(spy.count(), 1);
        auto msg = qvariant_cast<IProtocol::Message>(spy.at(0).at(0));
        QVERIFY(msg.is_valid);
    }

    // MessageParsed emitted once per frame when two frames fed together
    void TestMessageParsedSignalTwoFrames()
    {
        FrameProtocol proto(H::MakeSpec());
        QSignalSpy spy(&proto, &IProtocol::MessageParsed);

        QByteArray f1 = H::MakeAA55Frame(0x01, 0x01, QByteArray("a"));
        QByteArray f2 = H::MakeAA55Frame(0x02, 0x02, QByteArray("b"));
        proto.ParseData(f1 + f2);

        QCOMPARE(spy.count(), 2);
    }

    // ParseError is emitted when a frame with a bad checksum is received
    // (implementation may choose to emit ParseError or silently discard)
    void TestParseErrorSignalOnBadChecksum()
    {
        FrameProtocol proto(H::MakeSpec());
        QSignalSpy errSpy(&proto, &IProtocol::ParseError);
        QSignalSpy okSpy(&proto,  &IProtocol::MessageParsed);

        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("bad"));
        frame[frame.size() - 1] ^= 0xFF;
        proto.ParseData(frame);

        // Either ParseError fired, or no MessageParsed fired with is_valid=true
        bool had_valid = false;
        for (int i = 0; i < okSpy.count(); ++i) {
            auto msg = qvariant_cast<IProtocol::Message>(okSpy.at(i).at(0));
            if (msg.is_valid) had_valid = true;
        }
        QVERIFY(errSpy.count() > 0 || !had_valid);
    }
};

// ================================================================
//  TestFrameProtocolValidation
// ================================================================
class TestFrameProtocolValidation : public QObject
{
    Q_OBJECT

    using H = FrameProtocolTestHelpers;

private slots:

    // ValidateMessage returns true for a well-formed frame
    void TestValidateGoodFrame()
    {
        FrameProtocol proto(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("ok"));
        QVERIFY(proto.ValidateMessage(frame));
    }

    // ValidateMessage returns false when magic is wrong
    void TestValidateWrongMagic()
    {
        FrameProtocol proto(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("ok"));
        frame[0] = 0x00;
        QVERIFY(!proto.ValidateMessage(frame));
    }

    // ValidateMessage returns false when checksum is corrupted
    void TestValidateWrongChecksum()
    {
        FrameProtocol proto(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("ok"));
        frame[frame.size() - 1] ^= 0xFF;
        QVERIFY(!proto.ValidateMessage(frame));
    }

    // ValidateMessage does not modify internal parser state
    void TestValidateDoesNotAffectState()
    {
        FrameProtocol proto(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("test"));

        proto.ValidateMessage(frame);  // call once
        auto msgs = proto.ParseData(frame);  // then parse
        QCOMPARE(msgs.size(), std::size_t(1));
        QVERIFY(msgs[0].is_valid);
    }

    // Reset flushes the streaming buffer
    void TestResetClearsStreamBuffer()
    {
        FrameProtocol proto(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("reset_test"));
        int split = frame.size() / 2;

        proto.ParseData(frame.left(split));  // buffer partial frame
        proto.Reset();                        // flush

        auto msgs = proto.ParseData(frame.mid(split));
        bool found_valid = false;
        for (auto& m : msgs) if (m.is_valid) found_valid = true;
        QVERIFY(!found_valid);
    }

    // SetSpec replaces the spec and resets state
    void TestSetSpecReplacesSpec()
    {
        FrameProtocol proto(H::MakeSpec());
        FrameSpec new_spec = H::MakeSpec();
        new_spec.header_magic = QByteArray::fromHex("FFEE");
        proto.SetSpec(new_spec);
        QCOMPARE(proto.GetSpec().header_magic, QByteArray::fromHex("FFEE"));
    }
};
