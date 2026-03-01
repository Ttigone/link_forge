#pragma once

#include <QByteArray>
#include <QVariantMap>
#include <QString>
#include <vector>
#include <cstdint>

#include "../core/iprotocol.h"

namespace LinkForge {
namespace Protocol {

/**
 * @brief Specifies the structure of a binary serial data frame.
 *
 * Covers the vast majority of embedded serial frame formats:
 *
 *   [HEADER_MAGIC] [NAMED_FIELDS...] [LENGTH(1/2/4)] [DATA(LEN)] [CHECKSUM] [FOOTER_MAGIC]
 *
 * All integer fields are read/written big-endian by default; flip the
 * corresponding *_big_endian flag for little-endian protocols.
 *
 * Quick example -- a common embedded frame:
 *   0xAA 0x55 | ADDR(1) CMD(1) | LEN(2) | DATA(LEN) | CRC16(2)
 *
 * @code
 * FrameSpec spec;
 * spec.header_magic        = QByteArray::fromHex("AA55");
 * spec.named_fields        = { {"addr", 2, 1}, {"cmd", 3, 1} };
 * spec.length_field_offset = 4;
 * spec.length_field_size   = 2;
 * spec.data_offset         = 6;  // 2+1+1+2
 * spec.checksum_algorithm  = FrameSpec::ChecksumAlgorithm::CRC16_IBM;
 * spec.checksum_size       = 2;
 * @endcode
 */
struct FrameSpec {
    // ── Start marker ─────────────────────────────────────────────────────────
    /// Required bytes at the start.  Empty = no header sync.
    QByteArray header_magic;

    // ── Named fixed fields ────────────────────────────────────────────────────
    /**
     * Fixed-size fields embedded in the frame header (address, command, etc.).
     * The parser decodes each field and places it in the parsed message's
     * payload under its name key.
     */
    struct NamedField {
        QString name;
        int     offset;          ///< from frame start (0-based)
        int     size;            ///< bytes: 1-4
        bool    big_endian{true};
    };
    std::vector<NamedField> named_fields;

    // ── Length field ──────────────────────────────────────────────────────────
    /// Byte offset of the length field.  -1 = fixed-size frame; use fixed_data_size.
    int  length_field_offset{-1};
    int  length_field_size{2};       ///< bytes: 1, 2, or 4
    bool length_big_endian{true};
    /**
     * Adjustment applied to the decoded length value to obtain the number of
     * data bytes:  data_bytes = decoded_length + length_adjustment
     *
     * Use a negative value when the length field counts bytes that are not part
     * of the payload (e.g. the checksum is included in the length).
     */
    int  length_adjustment{0};

    // ── Data ─────────────────────────────────────────────────────────────────
    /// Byte offset in the frame where the variable-length data payload starts.
    int  data_offset{0};
    /// Payload size for fixed-size frames (length_field_offset == -1).
    int  fixed_data_size{-1};

    // ── Checksum ─────────────────────────────────────────────────────────────
    enum class ChecksumAlgorithm : quint8 {
        None        = 0,
        Sum8        = 1,   ///< 8-bit modular sum
        Sum16       = 2,   ///< 16-bit modular sum
        CRC16_IBM   = 3,   ///< CRC-16/IBM (Modbus RTU)
        CRC16_CCITT = 4,   ///< CRC-16/CCITT (X.25 / PPP)
        CRC32       = 5,   ///< IEEE 802.3 CRC-32
    };
    ChecksumAlgorithm checksum_algorithm{ChecksumAlgorithm::None};
    int  checksum_size{2};
    bool checksum_big_endian{true};
    int  checksum_range_begin{0};  ///< First covered byte (from frame start)
    int  checksum_range_end{-1};   ///< -1 = up to (not including) checksum field

    // ── End marker ────────────────────────────────────────────────────────────
    QByteArray footer_magic;

    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Total frame size for a frame with @p data_bytes of payload.
    [[nodiscard]] int FrameSize(int data_bytes) const noexcept;

    /// Minimum bytes to attempt parsing.
    [[nodiscard]] int MinFrameSize() const noexcept;

    /// Basic consistency check.
    [[nodiscard]] bool IsValid() const noexcept;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Stateful streaming frame parser driven by a FrameSpec.
 *
 * Feed incoming bytes incrementally; the parser buffers data internally and
 * returns one Core::IProtocol::Message per complete, valid frame.
 *
 * Invalid / unrecognised bytes are silently discarded (the parser advances
 * past them one byte at a time after header-sync fails).
 *
 * Thread safety: not thread-safe; protect externally when shared.
 */
class FrameParser {
public:
    explicit FrameParser(FrameSpec spec);
    FrameParser() = default;

    /// Feed new data into the parser.
    /// @return All complete frames decoded from the accumulated buffer.
    [[nodiscard]] std::vector<Core::IProtocol::Message>
        Feed(const QByteArray& data);

    void Reset();
    void SetSpec(FrameSpec spec);
    [[nodiscard]] const FrameSpec& GetSpec() const noexcept { return spec_; }

    // ── Standalone checksum utilities ─────────────────────────────────────
    [[nodiscard]] static quint32 ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                                  const QByteArray& data);
    [[nodiscard]] static quint32 ComputeChecksum(FrameSpec::ChecksumAlgorithm algo,
                                                  const QByteArray& data,
                                                  int begin, int end);

private:
    bool SyncToHeader();
    int  ExtractDataSize() const;
    int  ExpectedFrameSize(int data_size) const;
    bool VerifyChecksum(const QByteArray& frame) const;
    Core::IProtocol::Message ExtractMessage(const QByteArray& frame) const;

    static quint32 DecodeUInt(const char* buf, int size, bool big_endian) noexcept;
    static quint32 ComputeCRC16_IBM  (const QByteArray& data, int begin, int end);
    static quint32 ComputeCRC16_CCITT(const QByteArray& data, int begin, int end);
    static quint32 ComputeCRC32      (const QByteArray& data, int begin, int end);

    FrameSpec  spec_;
    QByteArray buffer_;
};

} // namespace Protocol
} // namespace LinkForge
