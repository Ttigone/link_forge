#include "custom_protocol.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace LinkForge {
namespace Protocol {

// ============================================================================
// CustomProtocol
// ============================================================================

CustomProtocol::CustomProtocol(QObject* parent)
    : IProtocol(parent)
{}

std::optional<QByteArray> CustomProtocol::BuildMessage(const QVariantMap& payload)
{
    QMutexLocker locker(&mutex_);

    QByteArray data = payload.value("data").toByteArray();
    if (data.isEmpty()) {
        const QString cmd = payload.value("command").toString();
            data = cmd.toUtf8();
        }
    }

    QByteArray frame;

    // Header
    frame.append(static_cast<char>(Frame::kHeader >> 8));
    frame.append(static_cast<char>(Frame::kHeader & 0xFF));

    // Length
    const quint16 length = static_cast<quint16>(data.size());
    frame.append(static_cast<char>(length >> 8));
    frame.append(static_cast<char>(length & 0xFF));

    // Data
    frame.append(data);

    // CRC (covers header + length + data)
    const quint16 crc = CalculateChecksum(frame);
    frame.append(static_cast<char>(crc >> 8));
    frame.append(static_cast<char>(crc & 0xFF));

    return frame;
}

std::vector<Core::IProtocol::Message> CustomProtocol::ParseData(const QByteArray& data)
{
    QMutexLocker locker(&mutex_);

    std::vector<Message> messages;
    receive_buffer_.append(data);

    while (receive_buffer_.size() >= Frame::kMinSize) {
        // Find frame header
        int header_pos = -1;
        for (int i = 0; i <= receive_buffer_.size() - 2; ++i) {
            const quint16 hdr = (static_cast<quint8>(receive_buffer_[i]) << 8) |
                                 static_cast<quint8>(receive_buffer_[i + 1]);
            if (hdr == Frame::kHeader) {
                header_pos = i;
                break;
            }
        }

        if (header_pos == -1) {
            receive_buffer_.clear();
            break;
        }
        if (header_pos > 0) {
            receive_buffer_.remove(0, header_pos);
        }

        if (receive_buffer_.size() < Frame::kHeaderSize + Frame::kLengthSize) {
            break;
        }

        const quint16 data_len = (static_cast<quint8>(receive_buffer_[2]) << 8) |
                                  static_cast<quint8>(receive_buffer_[3]);
        const int frame_len = Frame::kHeaderSize + Frame::kLengthSize +
                              data_len + Frame::kCrcSize;

        if (receive_buffer_.size() < frame_len) {
            break; // wait for more data
        }

        const QByteArray frame = receive_buffer_.left(frame_len);
        Message msg = ParseFrame(frame);
        if (msg.is_valid) {
            messages.push_back(msg);
            emit MessageParsed(msg);
        }
        receive_buffer_.remove(0, frame_len);
    }

    return messages;
}

bool CustomProtocol::ValidateMessage(const QByteArray& data) const
{
    if (data.size() < Frame::kMinSize) {
        return false;
    }
    const quint16 hdr = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);
    if (hdr != Frame::kHeader) {
        return false;
    }
    const quint16 data_len = (static_cast<quint8>(data[2]) << 8) |
                              static_cast<quint8>(data[3]);
    const int expected_len = Frame::kHeaderSize + Frame::kLengthSize +
                             data_len + Frame::kCrcSize;
    if (data.size() != expected_len) {
        return false;
    }
    const QByteArray body     = data.left(data.size() - Frame::kCrcSize);
    const quint16    recv_crc = (static_cast<quint8>(data[data.size() - 2]) << 8) |
                                 static_cast<quint8>(data[data.size() - 1]);
    return recv_crc == CalculateChecksum(body);
}

void CustomProtocol::Reset()
{
    QMutexLocker locker(&mutex_);
    receive_buffer_.clear();
}

quint16 CustomProtocol::CalculateChecksum(const QByteArray& data)
{
    quint16 sum = 0;
    for (const char byte : data) {
        sum += static_cast<quint8>(byte);
    }
    return sum;
}

Core::IProtocol::Message CustomProtocol::ParseFrame(const QByteArray& frame)
{
    Message msg;
    msg.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
    msg.raw_data     = frame;

        msg.is_valid = false;
        return msg;
    }

    const quint16 data_len = (static_cast<quint8>(frame[2]) << 8) |
                              static_cast<quint8>(frame[3]);
    const QByteArray payload_data = frame.mid(Frame::kHeaderSize + Frame::kLengthSize, data_len);

    msg.payload["data"]   = payload_data;
    msg.payload["length"] = data_len;
    msg.message_type     = QStringLiteral("CustomFrame");
    msg.is_valid         = true;
    return msg;
}

// ============================================================================
// JsonProtocol
// ============================================================================

JsonProtocol::JsonProtocol(QObject* parent)
    : IProtocol(parent)
{}

std::optional<QByteArray> JsonProtocol::BuildMessage(const QVariantMap& payload)
{
    QMutexLocker locker(&mutex_);

    const QJsonObject json = QJsonObject::fromVariantMap(payload);
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    data.append('
'); // newline delimiter
    return data;
}

std::vector<Core::IProtocol::Message> JsonProtocol::ParseData(const QByteArray& data)
{
    QMutexLocker locker(&mutex_);

    std::vector<Message> messages;
    receive_buffer_.append(data);

    while (true) {
        const int pos = receive_buffer_.indexOf('
');
        if (pos == -1) {
            break;
        }

        const QByteArray json_data = receive_buffer_.left(pos);
        receive_buffer_.remove(0, pos + 1);

        if (json_data.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parse_error;
        const QJsonDocument doc = QJsonDocument::fromJson(json_data, &parse_error);

        Message msg;
        msg.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
        msg.raw_data     = json_data;

        if (parse_error.error == QJsonParseError::NoError && doc.isObject()) {
            msg.payload      = doc.object().toVariantMap();
            msg.message_type = msg.payload.value("type", "Unknown").toString();
            msg.is_valid     = true;
            messages.push_back(msg);
            emit MessageParsed(msg);
        } else {
            msg.is_valid = false;
            emit ParseError(QString("JSON parse error: %1").arg(parse_error.errorString()));
        }
    }

    return messages;
}

bool JsonProtocol::ValidateMessage(const QByteArray& data) const
{
    QJsonParseError parse_error;
    QJsonDocument::fromJson(data, &parse_error);
    return parse_error.error == QJsonParseError::NoError;
}

void JsonProtocol::Reset()
{
    QMutexLocker locker(&mutex_);
    receive_buffer_.clear();
}

} // namespace Protocol
} // namespace LinkForge
