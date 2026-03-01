#pragma once

#include "../core/iprotocol.h"
#include <QMutex>

namespace LinkForge {
namespace Protocol {

/**
 * @brief Binary custom protocol.
 *
 * Frame format: [Header(2)][Length(2)][Data(n)][CRC(2)]
 * Header magic = 0xAA55.  CRC is a simple additive checksum.
 *
 * Intended as a template for project-specific binary protocols.
 */
class CustomProtocol : public Core::IProtocol {
    Q_OBJECT

public:
    struct Frame {
        static constexpr quint16 kHeader     = 0xAA55;
        static constexpr int     kHeaderSize = 2;
        static constexpr int     kLengthSize = 2;
        static constexpr int     kCrcSize    = 2;
        static constexpr int     kMinSize    = kHeaderSize + kLengthSize + kCrcSize;
    };

    explicit CustomProtocol(QObject* parent = nullptr);
    ~CustomProtocol() override = default;

    [[nodiscard]] std::optional<QByteArray> BuildMessage(const QVariantMap& payload) override;
    [[nodiscard]] std::vector<Message>       ParseData(const QByteArray& data)         override;
    [[nodiscard]] bool                       ValidateMessage(const QByteArray& data) const override;

    [[nodiscard]] Type    GetType() const override { return Type::Custom; }
    [[nodiscard]] QString GetName() const override { return QStringLiteral("Custom"); }
    void Reset() override;

private:
    [[nodiscard]] static quint16 CalculateChecksum(const QByteArray& data);
    [[nodiscard]] Message        ParseFrame(const QByteArray& frame);

    QByteArray receive_buffer_;
    QMutex     mutex_;
};

// ---------------------------------------------------------------------------

/**
 * @brief JSON text protocol.
 *
 * Each message is a compact JSON object terminated by a newline (
).
 * Suitable for debugging and human-readable wire protocols.
 */
class JsonProtocol : public Core::IProtocol {
    Q_OBJECT

public:
    explicit JsonProtocol(QObject* parent = nullptr);
    ~JsonProtocol() override = default;

    [[nodiscard]] std::optional<QByteArray> BuildMessage(const QVariantMap& payload) override;
    [[nodiscard]] std::vector<Message>       ParseData(const QByteArray& data)         override;
    [[nodiscard]] bool                       ValidateMessage(const QByteArray& data) const override;

    [[nodiscard]] Type    GetType() const override { return Type::Custom; }
    [[nodiscard]] QString GetName() const override { return QStringLiteral("JSON"); }
    void Reset() override;

private:
    QByteArray receive_buffer_;
    QMutex     mutex_;
};

} // namespace Protocol
} // namespace LinkForge
