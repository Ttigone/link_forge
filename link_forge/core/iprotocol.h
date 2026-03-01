#pragma once

#include <QByteArray>
#include <QObject>
#include <QVariantMap>
#include <memory>
#include <optional>
#include <vector>

namespace LinkForge {
namespace Core {

/**
 * @brief Protocol layer abstract interface.
 *
 * Defines the unified contract for all protocols (Modbus, MAVLink, UDS, etc.).
 * Responsible for encoding/decoding, validation, and framing.
 *
 * Naming conventions used throughout this project:
 *   - Member functions / static functions : PascalCase
 *   - Member variables                    : snake_case_  (trailing underscore)
 *   - Local variables / parameters        : snake_case   (no trailing underscore)
 */
class IProtocol : public QObject {
    Q_OBJECT

public:
    using Ptr     = std::shared_ptr<IProtocol>;
    using WeakPtr = std::weak_ptr<IProtocol>;

    enum class Type {
        Modbus,
        MAVLink,
        UDS,
        DBC,
        Custom,
        Unknown
    };
    Q_ENUM(Type)

    /// Wire-message container passed by value / const-ref across the stack.
    struct Message {
        QVariantMap payload;          ///< Structured, typed fields.
        QByteArray  raw_data;         ///< Raw bytes as received / sent on the wire.
        qint64      timestamp_ms{0};  ///< Epoch milliseconds at parse time.
        QString     message_type;     ///< Human-readable type tag.
        bool        is_valid{false};  ///< True when checksum / framing is good.
    };

    explicit IProtocol(QObject* parent = nullptr) : QObject(parent) {}
    ~IProtocol() override = default;

    IProtocol(const IProtocol&)            = delete;
    IProtocol& operator=(const IProtocol&) = delete;

    /**
     * @brief Encode a structured payload into a wire-format byte array.
     * @return Encoded bytes, or std::nullopt if encoding failed.
     */
    [[nodiscard]] virtual std::optional<QByteArray> BuildMessage(const QVariantMap& payload) = 0;

    /**
     * @brief Feed raw bytes into the protocol parser.
     * @return Zero or more fully parsed messages (streaming protocols may
     *         accumulate data across multiple calls before returning results).
     */
    [[nodiscard]] virtual std::vector<Message> ParseData(const QByteArray& data) = 0;

    /// Validate a candidate raw-byte frame (checksum, framing, etc.).
    [[nodiscard]] virtual bool ValidateMessage(const QByteArray& data) const = 0;

    [[nodiscard]] virtual Type    GetType() const = 0;
    [[nodiscard]] virtual QString GetName() const = 0;

    /// Flush internal reassembly buffers and reset incremental state.
    virtual void Reset() = 0;

signals:
    void MessageParsed(const LinkForge::Core::IProtocol::Message& message);
    void ParseError(const QString& error);
    void StatusUpdated(const QVariantMap& status);
};

} // namespace Core
} // namespace LinkForge
