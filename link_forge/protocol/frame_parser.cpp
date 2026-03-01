#include "frame_parser.h"

#include <QDateTime>
#include <QDebug>

namespace LinkForge {
namespace Protocol {

// ─────────────────────────────────────────────────────────────────────────────
// FrameSpec helpers
// ─────────────────────────────────────────────────────────────────────────────

int FrameSpec::FrameSize(int data_bytes) const noexcept
{
    const int cs = (checksum_algorithm != ChecksumAlgorithm::None) ? checksum_size : 0;
    return data_offset + data_bytes + cs + footer_magic.size();
}

int FrameSpec::MinFrameSize() const noexcept
{
    const int min_data = (fixed_data_size >= 0) ? fixed_data_size : 0;
    return FrameSize(min_data);
}

bool FrameSpec::IsValid() const noexcept
{
    if (data_offset < 0) return false;
    if (!header_magic.isEmpty() && data_offset < header_magic.size()) return false;
    if (length_field_offset >= 0) {
        if (length_field_offset + length_field_size > data_offset) return false;
        if (length_field_size < 1 || length_field_size > 4)        return false;
    } else {
        if (fixed_data_size < 0) return false;
    }
    if (checksum_algorithm != ChecksumAlgorithm::None) {
        if (checksum_size < 1 || checksum_size > 4) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FrameParser
// ─────────────────────────────────────────────────────────────────────────────

FrameParser::FrameParser(FrameSpec spec)
    : spec_(std::move(spec))
{}

void FrameParser::SetSpec(FrameSpec spec)
{
    spec_   = std::move(spec);
    buffer_.clear();
}

void FrameParser::Reset()
{
    buffer_.clear();
}

std::vector<Core::IProtocol::Message> FrameParser::Feed(const QByteArray& data)
{
    buffer_.append(data);

    std::vector<Core::IProtocol::Message> messages;

    while (!buffer_.isEmpty()) {
        // 1. Sync to frame header
        if (!SyncToHeader()) {
            break; // need more data
        }

        // 2. Determine data size
        const int data_size = ExtractDataSize();
        if (data_size < 0) {
            break; // length field not yet received
        }

        // 3. Wait for full frame
        const int frame_sz = ExpectedFrameSize(data_size);
        if (frame_sz < 0 || buffer_.size() < frame_sz) {
            break; // still incomplete
        }

        const QByteArray raw = buffer_.left(frame_sz);

        // 4. Verify footer
        if (!spec_.footer_magic.isEmpty()) {
            const int footer_pos = frame_sz - spec_.footer_magic.size();
            if (raw.mid(footer_pos) != spec_.footer_magic) {
                buffer_.remove(0, 1); // bad footer: skip one byte and resync
                continue;
            }
        }

        // 5. Verify checksum
        if (!VerifyChecksum(raw)) {
            qDebug() << "FrameParser: checksum mismatch, discarding frame";
            buffer_.remove(0, 1);
            continue;
        }

        // 6. Extract message
        messages.push_back(ExtractMessage(raw));
        buffer_.remove(0, frame_sz);
    }

    return messages;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FrameParser::SyncToHeader()
{
    if (spec_.header_magic.isEmpty()) {
        return true; // no header; always aligned
    }

    while (buffer_.size() >= spec_.header_magic.size()) {
        if (buffer_.startsWith(spec_.header_magic)) {
            return true;
        }
        buffer_.remove(0, 1); // skip one garbage byte
    }
    return false; // need more data
}

int FrameParser::ExtractDataSize() const
{
    if (spec_.length_field_offset < 0) {
        return spec_.fixed_data_size; // fixed-size frame
    }

    const int needed = spec_.length_field_offset + spec_.length_field_size;
    if (buffer_.size() < needed) {
        return -1; // not enough bytes yet
    }

    const quint32 raw_len = DecodeUInt(buffer_.constData() + spec_.length_field_offset,
                                        spec_.length_field_size,
                                        spec_.length_big_endian);

    return static_cast<int>(raw_len) + spec_.length_adjustment;
}

int FrameParser::ExpectedFrameSize(int data_size) const
{
    if (data_size < 0) return -1;
    return spec_.FrameSize(data_size);
}

bool FrameParser::VerifyChecksum(const QByteArray& frame) const
{
    if (spec_.checksum_algorithm == FrameSpec::ChecksumAlgorithm::None) {
        return true;
    }

    const int cs_pos  = frame.size() - spec_.footer_magic.size() - spec_.checksum_size;
    if (cs_pos < 0) return false;

    const int rng_end = (spec_.checksum_range_end < 0) ? cs_pos
                                                        : spec_.checksum_range_end;

    const quint32 computed = ComputeChecksum(spec_.checksum_algorithm,
                                              frame,
                                              spec_.checksum_range_begin,
                                              rng_end);
    const quint32 stored   = DecodeUInt(frame.constData() + cs_pos,
                                         spec_.checksum_size,
                                         spec_.checksum_big_endian);
    return computed == stored;
}

Core::IProtocol::Message FrameParser::ExtractMessage(const QByteArray& frame) const
{
    Core::IProtocol::Message msg;
    msg.raw_data     = frame;
    msg.is_valid     = true;
    msg.message_type = QStringLiteral("Frame");
    msg.timestamp_ms = QDateTime::currentMSecsSinceEpoch();

    // Extract data payload
    const int data_size = ExtractDataSize();
    if (data_size >= 0 && spec_.data_offset + data_size <= frame.size()) {
        msg.payload[QStringLiteral("data")] = frame.mid(spec_.data_offset, data_size);
    }

    // Extract named fixed fields
    for (const auto& field : spec_.named_fields) {
        if (field.offset + field.size <= frame.size()) {
            const quint32 val = DecodeUInt(frame.constData() + field.offset,
                                            field.size, field.big_endian);
            msg.payload[field.name] = val;
        }
    }

    return msg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Static -- integer decode
// ─────────────────────────────────────────────────────────────────────────────

quint32 FrameParser::DecodeUInt(const char* buf, int size, bool big_endian) noexcept
{
    quint32 val = 0;
    if (big_endian) {
        for (int i = 0; i < size; ++i) {
            val = (val << 8) | static_cast<quint8>(buf[i]);
        }
    } else {
        for (int i = size - 1; i >= 0; --i) {
            val = (val << 8) | static_cast<quint8>(buf[i]);
        }
    }
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Static -- public checksum dispatch
// ─────────────────────────────────────────────────────────────────────────────

quint32 FrameParser::ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                      const QByteArray& data)
{
    return ComputeChecksum(algo, data, 0, data.size());
}

quint32 FrameParser::ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                      const QByteArray& data,
                                      int begin, int end)
{
    using Algo = FrameSpec::ChecksumAlgorithm;

    if (begin < 0) begin = 0;
    if (end > data.size()) end = data.size();
    if (begin >= end) return 0;

    switch (algo) {
    case Algo::None:
        return 0;

    case Algo::Sum8: {
        quint8 s = 0;
        for (int i = begin; i < end; ++i) {
            s = static_cast<quint8>(s + static_cast<quint8>(data[i]));
        }
        return s;
    }

    case Algo::Sum16: {
        quint16 s = 0;
        for (int i = begin; i < end; ++i) {
            s = static_cast<quint16>(s + static_cast<quint8>(data[i]));
        }
        return s;
    }

    case Algo::CRC16_IBM:
        return ComputeCRC16_IBM(data, begin, end);

    case Algo::CRC16_CCITT:
        return ComputeCRC16_CCITT(data, begin, end);

    case Algo::CRC32:
        return ComputeCRC32(data, begin, end);
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// CRC algorithms
// ─────────────────────────────────────────────────────────────────────────────

quint32 FrameParser::ComputeCRC16_IBM(const QByteArray& data, int begin, int end)
{
    // Poly 0x8005, init 0xFFFF, reflected input/output (Modbus RTU)
    quint16 crc = 0xFFFF;
    for (int i = begin; i < end; ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x0001u) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001u);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

quint32 FrameParser::ComputeCRC16_CCITT(const QByteArray& data, int begin, int end)
{
    // Poly 0x1021, init 0xFFFF, non-reflected (X.25 / PPP)
    quint16 crc = 0xFFFF;
    for (int i = begin; i < end; ++i) {
        crc ^= static_cast<quint16>(static_cast<quint8>(data[i]) << 8);
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000u) {
                crc = static_cast<quint16>((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

quint32 FrameParser::ComputeCRC32(const QByteArray& data, int begin, int end)
{
    // IEEE 802.3, poly 0x04C11DB7, reflected
    quint32 crc = 0xFFFFFFFFu;
    for (int i = begin; i < end; ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int b = 0; b < 8; ++b) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace Protocol
} // namespace LinkForge
