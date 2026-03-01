#pragma once

#include <QMutex>
#include <memory>

#include "../core/iprotocol.h"
#include "frame_parser.h"

namespace LinkForge {
namespace Protocol {

/**
 * @brief IProtocol adapter that uses a FrameSpec for encoding and decoding.
 *
 * This is the primary entry point for custom serial data frame protocol parsing.
 * Define a FrameSpec that matches your device's wire format, construct a
 * FrameProtocol with that spec, and hand it to a CommunicationManager or
 * ChannelManager.
 *
 * BuildMessage() encodes a QVariantMap payload into a raw binary frame:
 *   - Writes header_magic
 *   - Writes each named_field value from payload[field.name]
 *   - Writes the length field (data bytes count)
 *   - Appends data bytes (from payload["data"] as QByteArray)
 *   - Appends checksum
 *   - Appends footer_magic
 *
 * ParseData() feeds raw bytes to an internal FrameParser and returns all
 * complete frames decoded.  It also emits MessageParsed for each one.
 *
 * Thread safety: ParseData / Reset are guarded by a QMutex and safe to call
 * from the I/O thread while BuildMessage is called from the GUI thread.
 *
 * Runtime construction from a config map:
 * @code
 * QVariantMap cfg;
 * cfg["headerMagic"]        = "AA55";          // hex string
 * cfg["lengthFieldOffset"]  = 4;
 * cfg["lengthFieldSize"]    = 2;
 * cfg["dataOffset"]         = 6;
 * cfg["checksumAlgorithm"]  = "crc16ibm";
 * cfg["checksumSize"]       = 2;
 * auto proto = FrameProtocol::FromConfig(cfg);
 * @endcode
 *
 * Factory registration example:
 * @code
 * ProtocolFactory::RegisterProtocol("Frame",
 *     [spec](const QVariantMap& cfg, QObject* p) {
 *         return FrameProtocol::FromConfig(cfg, p);
 *     });
 * @endcode
 */
class FrameProtocol : public Core::IProtocol {
    Q_OBJECT

public:
    explicit FrameProtocol(FrameSpec spec, QObject* parent = nullptr);
    ~FrameProtocol() override = default;

    // ── IProtocol ──────────────────────────────────────────────────────────
    [[nodiscard]] std::optional<QByteArray>
        BuildMessage(const QVariantMap& payload) override;

    [[nodiscard]] std::vector<Message>
        ParseData(const QByteArray& data) override;

    [[nodiscard]] bool ValidateMessage(const QByteArray& data) const override;

    [[nodiscard]] Type    GetType() const override;
    [[nodiscard]] QString GetName() const override;

    void Reset() override;

    // ── FrameProtocol-specific ─────────────────────────────────────────────

    void SetSpec(FrameSpec spec);
    [[nodiscard]] const FrameSpec& GetSpec() const;

    /**
     * @brief Construct a FrameProtocol from a QVariantMap description.
     *
     * Recognised keys:
     *   "headerMagic"         - hex string, e.g. "AA55"
     *   "footerMagic"         - hex string (optional)
     *   "lengthFieldOffset"   - int (-1 = fixed-size)
     *   "lengthFieldSize"     - int (1/2/4, default 2)
     *   "lengthBigEndian"     - bool (default true)
     *   "lengthAdjustment"    - int (default 0)
     *   "dataOffset"          - int
     *   "fixedDataSize"       - int (-1 = variable, default -1)
     *   "checksumAlgorithm"   - "none"|"sum8"|"sum16"|"crc16ibm"|"crc16ccitt"|"crc32"
     *   "checksumSize"        - int (1/2/4, default 2)
     *   "checksumBigEndian"   - bool (default true)
     *   "checksumRangeBegin"  - int (default 0)
     *   "checksumRangeEnd"    - int (-1 = auto, default -1)
     *   "namedFields"         - QVariantList of {name, offset, size, bigEndian}
     */
    [[nodiscard]] static std::shared_ptr<FrameProtocol>
        FromConfig(const QVariantMap& config, QObject* parent = nullptr);

private:
    [[nodiscard]] static FrameSpec SpecFromConfig(const QVariantMap& config);

    mutable QMutex mutex_;
    FrameParser    parser_;
    FrameSpec      spec_;
};

} // namespace Protocol
} // namespace LinkForge
