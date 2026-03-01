#pragma once

#include <QtTest/QtTest>
#include <QByteArray>
#include <QVariantMap>

#include "protocol/frame_parser.h"

using namespace LinkForge::Core;

// ----------------------------------------------------------------
//  Local helpers
// ----------------------------------------------------------------
namespace FrameParserTestHelpers {

/**
 * Compute CRC-16/IBM (poly 0x8005, init 0xFFFF) over a byte range.
 * This is an independent reference implementation used to cross-check
 * FrameParser::ComputeChecksum.
 */
inline quint16 RefCRC16IBM(const QByteArray& data, int begin = 0, int end = -1)
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
 * Build: AA55 | addr(1) | cmd(1) | len-BE(2) | data | CRC16IBM-BE(2)
 */
inline QByteArray MakeAA55Frame(quint8 addr, quint8 cmd, const QByteArray& data)
{
    QByteArray f;
    f.append(char(0xAA));
    f.append(char(0x55));
    f.append(char(addr));
    f.append(char(cmd));
    quint16 len = quint16(data.size());
    f.append(char((len >> 8) & 0xFF));
    f.append(char(len & 0xFF));
    f.append(data);
    quint16 crc = RefCRC16IBM(f);
    f.append(char((crc >> 8) & 0xFF));
    f.append(char(crc & 0xFF));
    return f;
}

/**
 * The canonical FrameSpec used throughout these tests:
 *   AA55 | addr(1B offset 2) | cmd(1B offset 3) | len-BE(2) | data | CRC16IBM-BE(2)
 */
inline FrameSpec MakeSpec()
{
    FrameSpec s;
    s.header_magic        = QByteArray::fromHex("AA55");
    s.named_fields        = { {"addr", 2, 1, true}, {"cmd", 3, 1, true} };
    s.length_field_offset = 4;
    s.length_field_size   = 2;
    s.length_big_endian   = true;
    s.length_adjustment   = 0;
    s.data_offset         = 6;
    s.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::CRC16_IBM;
    s.checksum_size       = 2;
    s.checksum_big_endian = true;
    return s;
}

} // namespace FrameParserTestHelpers

// ================================================================
//  TestFrameSpec
// ================================================================
class TestFrameSpec : public QObject
{
    Q_OBJECT

private slots:

    // IsValid rejects obviously broken specs
    void TestIsValidReturnsFalseForEmptySpec()
    {
        FrameSpec empty;
        // data_offset == length_field_offset == -1  →  invalid
        QVERIFY(!empty.IsValid());
    }

    void TestIsValidReturnsTrueForWellFormedSpec()
    {
        QVERIFY(FrameParserTestHelpers::MakeSpec().IsValid());
    }

    // MinFrameSize: header(2) + addr(1) + cmd(1) + length-field(2) + data(0) + crc(2) = 8
    void TestMinFrameSize()
    {
        FrameSpec s = FrameParserTestHelpers::MakeSpec();
        // With 0 data bytes the frame is: AA55(2) addr(1) cmd(1) len(2) crc(2) = 8
        QCOMPARE(s.MinFrameSize(), 8);
    }

    // FrameSize(n) = MinFrameSize + n
    void TestFrameSize()
    {
        FrameSpec s = FrameParserTestHelpers::MakeSpec();
        QCOMPARE(s.FrameSize(4), s.MinFrameSize() + 4);
        QCOMPARE(s.FrameSize(0), s.MinFrameSize());
    }

    // Fixed-size spec has a single unambiguous frame size
    void TestFixedSizeSpec()
    {
        FrameSpec s;
        s.header_magic       = QByteArray::fromHex("AA55");
        s.length_field_offset = -1;   // no length field
        s.fixed_data_size    = 8;
        s.data_offset        = 2;
        s.checksum_algorithm = FrameSpec::ChecksumAlgorithm::None;
        QVERIFY(s.IsValid());
        // frame = header(2) + data(8) = 10
        QCOMPARE(s.FrameSize(8), 10);
    }
};

// ================================================================
//  TestFrameParserChecksums
// ================================================================
class TestFrameParserChecksums : public QObject
{
    Q_OBJECT

private slots:

    // CRC16_IBM: Modbus known-good test vector
    // 0x01 0x03 0x00 0x00 0x00 0x02  →  CRC = 0xC40B
    void TestCRC16IBM_KnownVector()
    {
        QByteArray data = QByteArray::fromHex("010300000002");
        quint32 result  = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC16_IBM, data);
        QCOMPARE(result, quint32(0xC40B));
    }

    // CRC16_IBM over a sub-range
    void TestCRC16IBM_SubRange()
    {
        QByteArray data = QByteArray::fromHex("FF010300000002FF");
        quint32 result  = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC16_IBM, data, 1, 7);
        quint32 expected = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC16_IBM,
            QByteArray::fromHex("010300000002"));
        QCOMPARE(result, expected);
    }

    // Sum8: 0x01+0x02+0x03+0x04 = 0x0A
    void TestSum8()
    {
        QByteArray data = QByteArray::fromHex("01020304");
        quint32 result  = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::Sum8, data);
        QCOMPARE(result, quint32(0x0A));
    }

    // Sum8 overflow wraps at 8 bits
    void TestSum8Overflow()
    {
        QByteArray data;
        data.fill(char(0xFF), 4);  // 4 * 255 = 1020 → 0xFC mod 256
        quint32 result = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::Sum8, data);
        QCOMPARE(result, quint32(0xFC));
    }

    // Sum16
    void TestSum16()
    {
        QByteArray data = QByteArray::fromHex("01020304");
        quint32 result  = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::Sum16, data);
        QCOMPARE(result, quint32(0x000A));
    }

    // None algorithm always returns 0
    void TestNone()
    {
        QByteArray data = QByteArray("irrelevant bytes");
        quint32 result  = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::None, data);
        QCOMPARE(result, quint32(0));
    }

    // CRC32: validate self-consistency (encode then re-check)
    void TestCRC32SelfConsistency()
    {
        QByteArray data = QByteArray("link_forge_test_payload");
        quint32 a = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC32, data);
        quint32 b = FrameParser::ComputeChecksum(
            FrameSpec::ChecksumAlgorithm::CRC32, data);
        QCOMPARE(a, b);  // deterministic
        QVERIFY(a != 0);
    }
};

// ================================================================
//  TestFrameParserStreaming
// ================================================================
class TestFrameParserStreaming : public QObject
{
    Q_OBJECT

    using H = FrameParserTestHelpers;

private slots:

    // Single complete frame
    void TestSingleFrame()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x02, QByteArray("hello"));

        auto results = p.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QVERIFY(results[0].is_valid);
        QCOMPARE(results[0].raw_data, frame);
        QCOMPARE(results[0].message_type, QString("Frame"));
    }

    // Split at every possible byte boundary
    void TestAllSplitPositions()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("split"));

        for (int split = 1; split < frame.size(); ++split) {
            p.Reset();
            auto r1 = p.Feed(frame.left(split));
            auto r2 = p.Feed(frame.mid(split));

            QVERIFY2(r1.empty() || (!r1.empty() && r2.empty()),
                     qPrintable(QString("split=%1").arg(split)));

            bool found_valid = false;
            for (auto& m : r1) if (m.is_valid) found_valid = true;
            for (auto& m : r2) if (m.is_valid) found_valid = true;
            QVERIFY2(found_valid, qPrintable(QString("split=%1 produced no valid frame").arg(split)));
        }
    }

    // Two frames in one Feed call
    void TestTwoFramesInOneFeed()
    {
        FrameParser p(H::MakeSpec());
        QByteArray f1 = H::MakeAA55Frame(0x01, 0x01, QByteArray("first"));
        QByteArray f2 = H::MakeAA55Frame(0x02, 0x02, QByteArray("second"));

        auto results = p.Feed(f1 + f2);
        QCOMPARE(results.size(), std::size_t(2));
        QVERIFY(results[0].is_valid);
        QVERIFY(results[1].is_valid);
    }

    // Three frames in one Feed call
    void TestThreeFrames()
    {
        FrameParser p(H::MakeSpec());
        QByteArray f1 = H::MakeAA55Frame(0x01, 0x01, QByteArray("a"));
        QByteArray f2 = H::MakeAA55Frame(0x02, 0x02, QByteArray("bb"));
        QByteArray f3 = H::MakeAA55Frame(0x03, 0x03, QByteArray("ccc"));

        auto results = p.Feed(f1 + f2 + f3);
        QCOMPARE(results.size(), std::size_t(3));
    }

    // Empty data feed does not crash and returns nothing
    void TestEmptyFeed()
    {
        FrameParser p(H::MakeSpec());
        auto results = p.Feed(QByteArray());
        QVERIFY(results.empty());
    }

    // Feed garbage bytes before a valid frame; parser must skip and find the frame
    void TestGarbageBeforeFrame()
    {
        FrameParser p(H::MakeSpec());
        QByteArray garbage = QByteArray::fromHex("112233445566");
        QByteArray frame   = H::MakeAA55Frame(0x01, 0x01, QByteArray("real"));

        auto results = p.Feed(garbage + frame);
        bool found = false;
        for (auto& m : results) if (m.is_valid) found = true;
        QVERIFY(found);
    }

    // Reset flushes the reassembly buffer
    void TestResetFlushesBuffer()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("test"));
        int split = frame.size() / 2;

        p.Feed(frame.left(split));  // buffer partial frame
        p.Reset();                  // flush

        // Feed only the second half; cannot form a valid frame without the first half
        auto results = p.Feed(frame.mid(split));
        bool found_valid = false;
        for (auto& m : results) if (m.is_valid) found_valid = true;
        QVERIFY(!found_valid);
    }

    // Feed across 5 incremental chunks
    void TestIncrementalFeed()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0xAB, 0xCD, QByteArray("incremental"));

        std::vector<IProtocol::Message> all_results;
        int chunk_size = (frame.size() + 4) / 5;
        for (int i = 0; i < frame.size(); i += chunk_size) {
            auto r = p.Feed(frame.mid(i, chunk_size));
            all_results.insert(all_results.end(), r.begin(), r.end());
        }

        QCOMPARE(all_results.size(), std::size_t(1));
        QVERIFY(all_results[0].is_valid);
    }
};

// ================================================================
//  TestFrameParserNamedFields
// ================================================================
class TestFrameParserNamedFields : public QObject
{
    Q_OBJECT

    using H = FrameParserTestHelpers;

private slots:

    // addr and cmd fields are decoded correctly from the spec
    void TestAddrAndCmdDecoded()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0xAB, 0xCD, QByteArray("fields"));

        auto results = p.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QCOMPARE(results[0].payload.value("addr").toUInt(), quint32(0xAB));
        QCOMPARE(results[0].payload.value("cmd").toUInt(),  quint32(0xCD));
    }

    // Data payload is placed under key "data"
    void TestDataPayloadKey()
    {
        FrameParser p(H::MakeSpec());
        QByteArray payload = QByteArray::fromHex("DEADBEEF");
        auto results = p.Feed(H::MakeAA55Frame(0x01, 0x01, payload));

        QCOMPARE(results.size(), std::size_t(1));
        QCOMPARE(results[0].payload.value("data").toByteArray(), payload);
    }

    // Two-byte big-endian field
    void TestUInt16BEField()
    {
        FrameSpec s;
        s.header_magic        = QByteArray::fromHex("AA55");
        s.named_fields        = { {"voltage", 2, 2, true} };  // 2B at offset 2, big-endian
        s.length_field_offset = 4;
        s.length_field_size   = 2;
        s.data_offset         = 6;
        s.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::None;

        FrameParser p(s);

        QByteArray frame;
        frame += QByteArray::fromHex("AA55");  // header
        frame += char(0x13); frame += char(0x88);  // voltage = 0x1388 = 5000
        frame += char(0x00); frame += char(0x02);  // length = 2
        frame += char(0xAA); frame += char(0xBB);  // data

        auto results = p.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QCOMPARE(results[0].payload.value("voltage").toUInt(), quint32(5000));
    }

    // Two-byte little-endian field
    void TestUInt16LEField()
    {
        FrameSpec s;
        s.header_magic        = QByteArray::fromHex("AA55");
        s.named_fields        = { {"speed", 2, 2, false} };  // little-endian
        s.length_field_offset = 4;
        s.length_field_size   = 2;
        s.data_offset         = 6;
        s.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::None;

        FrameParser p(s);

        QByteArray frame;
        frame += QByteArray::fromHex("AA55");
        frame += char(0x88); frame += char(0x13);  // LE: 0x1388 = 5000
        frame += char(0x00); frame += char(0x01);  // length = 1
        frame += char(0xFF);                        // data

        auto results = p.Feed(frame);
        QCOMPARE(results.size(), std::size_t(1));
        QCOMPARE(results[0].payload.value("speed").toUInt(), quint32(5000));
    }

    // Zero-length data payload is valid
    void TestEmptyDataPayload()
    {
        FrameParser p(H::MakeSpec());
        auto results = p.Feed(H::MakeAA55Frame(0x01, 0x01, QByteArray()));
        QCOMPARE(results.size(), std::size_t(1));
        QVERIFY(results[0].is_valid);
        QVERIFY(results[0].payload.value("data").toByteArray().isEmpty());
    }
};

// ================================================================
//  TestFrameParserInvalidFrames
// ================================================================
class TestFrameParserInvalidFrames : public QObject
{
    Q_OBJECT

    using H = FrameParserTestHelpers;

private slots:

    // Wrong checksum: is_valid must be false (or frame rejected entirely)
    void TestWrongChecksum()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("abc"));
        frame[frame.size() - 1] ^= 0xFF;  // flip low CRC byte

        auto results = p.Feed(frame);
        if (!results.empty()) {
            QVERIFY(!results[0].is_valid);
        }
        // empty result is also acceptable (parser discards invalid frames)
    }

    // Truncated frame: Feed the frame minus the last byte — no valid result
    void TestTruncatedFrame()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("xyz"));
        auto results = p.Feed(frame.left(frame.size() - 1));
        bool found_valid = false;
        for (auto& m : results) if (m.is_valid) found_valid = true;
        QVERIFY(!found_valid);
    }

    // Frame with wrong magic is not consumed as a valid frame
    void TestWrongHeaderMagic()
    {
        FrameParser p(H::MakeSpec());
        QByteArray frame = H::MakeAA55Frame(0x01, 0x01, QByteArray("test"));
        frame[0] = 0x00;  // corrupt first magic byte

        auto results = p.Feed(frame);
        bool found_valid = false;
        for (auto& m : results) if (m.is_valid) found_valid = true;
        QVERIFY(!found_valid);
    }

    // SetSpec replaces spec and implicitly resets
    void TestSetSpecReplacesSpec()
    {
        FrameSpec s1 = H::MakeSpec();
        FrameParser p(s1);

        FrameSpec s2;
        s2.header_magic        = QByteArray::fromHex("FFEE");
        s2.length_field_offset = 2;
        s2.length_field_size   = 1;
        s2.data_offset         = 3;
        s2.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::None;

        p.SetSpec(s2);
        QCOMPARE(p.GetSpec().header_magic, QByteArray::fromHex("FFEE"));
    }
};
