#include "frame_protocol.h"

#include <QDateTime>
#include <QDebug>
#include <unordered_map>
#include <cstring>

namespace LinkForge {
namespace Protocol {

using namespace Core;

FrameProtocol::FrameProtocol(FrameSpec spec, QObject* parent)
    : IProtocol(parent)
    , parser_(spec)
    , spec_(std::move(spec))
{}

// ────────────────────────────────────────────────────────────────────────────
// IProtocol
// ────────────────────────────────────────────────────────────────────────────

std::optional<QByteArray> FrameProtocol::BuildMessage(const QVariantMap& payload)
{
    if (!spec_.IsValid()) {
        emit ParseError(QStringLiteral("FrameProtocol: invalid FrameSpec"));
        return std::nullopt;
    }

    const QByteArray data      = payload.value(QStringLiteral("data")).toByteArray();
    const int        data_size = data.size();
    const int        frame_sz  = spec_.FrameSize(data_size);

    QByteArray frame(frame_sz, '\0');

    // ── Header magic ─────────────────────────────────────────────────────
    if (!spec_.header_magic.isEmpty()) {
        std::memcpy(frame.data(), spec_.header_magic.constData(),
                    static_cast<size_t>(spec_.header_magic.size()));
    }

    // ── Named fixed fields ────────────────────────────────────────────────
    for (const auto& field : spec_.named_fields) {
        if (field.offset + field.size > frame_sz) continue;

        const quint32 val = payload.value(field.name, 0u).toUInt();
        auto* p = reinterpret_cast<quint8*>(frame.data()) + field.offset;

        if (field.big_endian) {
            for (int i = 0; i < field.size; ++i) {
                p[i] = static_cast<quint8>(val >> (8 * (field.size - 1 - i)));
            }
        } else {
            for (int i = 0; i < field.size; ++i) {
                p[i] = static_cast<quint8>(val >> (8 * i));
            }
        }
    }

    // ── Length field ─────────────────────────────────────────────────────
    if (spec_.length_field_offset >= 0) {
        const quint32 len_val =
            static_cast<quint32>(data_size - spec_.length_adjustment);
        auto* p = reinterpret_cast<quint8*>(frame.data()) + spec_.length_field_offset;

        if (spec_.length_big_endian) {
            for (int i = 0; i < spec_.length_field_size; ++i) {
                p[i] = static_cast<quint8>(
                    len_val >> (8 * (spec_.length_field_size - 1 - i)));
            }
        } else {
            for (int i = 0; i < spec_.length_field_size; ++i) {
                p[i] = static_cast<quint8>(len_val >> (8 * i));
            }
        }
    }

    // ── Data payload ─────────────────────────────────────────────────────
    if (data_size > 0) {
        std::memcpy(frame.data() + spec_.data_offset,
                    data.constData(), static_cast<size_t>(data_size));
    }

    // ── Checksum ─────────────────────────────────────────────────────────
    const bool has_cs = (spec_.checksum_algorithm != FrameSpec::ChecksumAlgorithm::None);
    if (has_cs) {
        const int cs_pos  = spec_.data_offset + data_size;
        const int rng_end = (spec_.checksum_range_end < 0) ? cs_pos
                                                            : spec_.checksum_range_end;

        const quint32 cs = FrameParser::ComputeChecksum(spec_.checksum_algorithm,
                                                         frame,
                                                         spec_.checksum_range_begin,
                                                         rng_end);
        auto* p = reinterpret_cast<quint8*>(frame.data()) + cs_pos;

        if (spec_.checksum_big_endian) {
            for (int i = 0; i < spec_.checksum_size; ++i) {
                p[i] = static_cast<quint8>(
                    cs >> (8 * (spec_.checksum_size - 1 - i)));
            }
        } else {
            for (int i = 0; i < spec_.checksum_size; ++i) {
                p[i] = static_cast<quint8>(cs >> (8 * i));
            }
        }
    }

    // ── Footer magic ─────────────────────────────────────────────────────
    if (!spec_.footer_magic.isEmpty()) {
        const int footer_pos = frame_sz - spec_.footer_magic.size();
        std::memcpy(frame.data() + footer_pos,
                    spec_.footer_magic.constData(),
                    static_cast<size_t>(spec_.footer_magic.size()));
    }

    return frame;
}

std::vector<IProtocol::Message> FrameProtocol::ParseData(const QByteArray& data)
{
    QMutexLocker lock(&mutex_);
    auto messages = parser_.Feed(data);
    for (const auto& msg : messages) {
        emit MessageParsed(msg);
    }
    return messages;
}

bool FrameProtocol::ValidateMessage(const QByteArray& data) const
{
    // Run the parser in a one-shot fashion on a copy (no side effects).
    FrameParser tmp(spec_);
    const auto result = tmp.Feed(data);
    return !result.empty() && result.front().is_valid;
}

IProtocol::Type FrameProtocol::GetType() const
{
    return Type::Custom;
}

QString FrameProtocol::GetName() const
{
    return QStringLiteral("Frame");
}

void FrameProtocol::Reset()
{
    QMutexLocker lock(&mutex_);
    parser_.Reset();
    emit StatusUpdated({{QStringLiteral("message"), QStringLiteral("FrameProtocol reset")}});
}

// ────────────────────────────────────────────────────────────────────────────
// FrameProtocol-specific
// ────────────────────────────────────────────────────────────────────────────

void FrameProtocol::SetSpec(FrameSpec spec)
{
    QMutexLocker lock(&mutex_);
    spec_ = spec;
    parser_.SetSpec(std::move(spec));
}

const FrameSpec& FrameProtocol::GetSpec() const
{
    return spec_;
}

std::shared_ptr<FrameProtocol>
FrameProtocol::FromConfig(const QVariantMap& config, QObject* parent)
{
    return std::make_shared<FrameProtocol>(SpecFromConfig(config), parent);
}

FrameSpec FrameProtocol::SpecFromConfig(const QVariantMap& config)
{
    FrameSpec spec;

    spec.header_magic = QByteArray::fromHex(
        config.value(QStringLiteral("headerMagic")).toString().toLatin1());
    spec.footer_magic = QByteArray::fromHex(
        config.value(QStringLiteral("footerMagic")).toString().toLatin1());

    spec.length_field_offset = config.value(QStringLiteral("lengthFieldOffset"), -1).toInt();
    spec.length_field_size   = config.value(QStringLiteral("lengthFieldSize"),    2).toInt();
    spec.length_big_endian   = config.value(QStringLiteral("lengthBigEndian"),    true).toBool();
    spec.length_adjustment   = config.value(QStringLiteral("lengthAdjustment"),   0).toInt();
    spec.data_offset         = config.value(QStringLiteral("dataOffset"),         0).toInt();
    spec.fixed_data_size     = config.value(QStringLiteral("fixedDataSize"),     -1).toInt();

    // Checksum algorithm from string
    const QString algo_str = config.value(QStringLiteral("checksumAlgorithm"),
                                           QStringLiteral("none")).toString().toLower();
    static const std::unordered_map<std::string, FrameSpec::ChecksumAlgorithm> kAlgoMap{
        {"none",       FrameSpec::ChecksumAlgorithm::None},
        {"sum8",       FrameSpec::ChecksumAlgorithm::Sum8},
        {"sum16",      FrameSpec::ChecksumAlgorithm::Sum16},
        {"crc16ibm",   FrameSpec::ChecksumAlgorithm::CRC16_IBM},
        {"crc16ccitt", FrameSpec::ChecksumAlgorithm::CRC16_CCITT},
        {"crc32",      FrameSpec::ChecksumAlgorithm::CRC32},
    };
    if (const auto it = kAlgoMap.find(algo_str.toStdString()); it != kAlgoMap.end()) {
        spec.checksum_algorithm = it->second;
    }

    spec.checksum_size        = config.value(QStringLiteral("checksumSize"),       2).toInt();
    spec.checksum_big_endian  = config.value(QStringLiteral("checksumBigEndian"),  true).toBool();
    spec.checksum_range_begin = config.value(QStringLiteral("checksumRangeBegin"), 0).toInt();
    spec.checksum_range_end   = config.value(QStringLiteral("checksumRangeEnd"),  -1).toInt();

    // Named fields
    const auto fields_list = config.value(QStringLiteral("namedFields")).toList();
    for (const QVariant& fv : fields_list) {
        const QVariantMap fm = fv.toMap();
        FrameSpec::NamedField f;
        f.name       = fm.value(QStringLiteral("name")).toString();
        f.offset     = fm.value(QStringLiteral("offset"),    0).toInt();
        f.size       = fm.value(QStringLiteral("size"),      1).toInt();
        f.big_endian = fm.value(QStringLiteral("bigEndian"), true).toBool();
        if (!f.name.isEmpty() && f.size >= 1 && f.size <= 4) {
            spec.named_fields.push_back(std::move(f));
        }
    }

    return spec;
}

} // namespace Protocol
} // namespace LinkForge
