#pragma once

#include <QAbstractTableModel>
#include <QObject>
#include <deque>

#include "../core/communication_manager.h"

namespace LinkForge {
namespace Application {

/**
 * @brief Qt table model for displaying sent / received messages in a view.
 */
class MessageModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Timestamp, Direction, Type, Data, ColumnCount };

    struct MessageRecord {
        qint64     timestamp_ms;
        QString    direction;   ///< "RX" or "TX"
        QString    type;
        QString    data;        ///< Compact JSON / hex representation.
        QByteArray raw_data;
    };

    explicit MessageModel(QObject* parent = nullptr);

    int     rowCount(const QModelIndex& parent    = {}) const override;
    int     columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void AddReceivedMessage(const Core::IProtocol::Message& message);
    void AddSentMessage(const QVariantMap& payload, const QByteArray& raw_data);

    void Clear();
    void SetMaxMessages(int max) { max_messages_ = max; }
    [[nodiscard]] const std::deque<MessageRecord>& GetMessages() const { return messages_; }

private:
    std::deque<MessageRecord> messages_;
    int max_messages_{1000};
};

} // namespace Application
} // namespace LinkForge
