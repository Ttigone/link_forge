#include "message_model.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace LinkForge {
namespace Application {

MessageModel::MessageModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

int MessageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(messages_.size());
}

int MessageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MessageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }

    const MessageRecord& rec = messages_[index.row()];
    switch (index.column()) {
        case Timestamp:
            return QDateTime::fromMSecsSinceEpoch(rec.timestamp_ms).toString("hh:mm:ss.zzz");
        case Direction:
            return rec.direction;
        case Type:
            return rec.type;
        case Data:
            return rec.data;
        default:
            return {};
    }
}

QVariant MessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
        case Timestamp:  return tr("Timestamp");
        case Direction:  return tr("Direction");
        case Type:       return tr("Type");
        case Data:       return tr("Data");
        default:         return {};
    }
}

void MessageModel::AddReceivedMessage(const Core::IProtocol::Message& message)
{
    MessageRecord rec;
    rec.timestamp_ms = message.timestamp_ms;
    rec.direction    = QStringLiteral("RX");
    rec.type         = message.message_type;
    rec.raw_data     = message.raw_data;
    rec.data         = QJsonDocument::fromVariant(message.payload).toJson(QJsonDocument::Compact);

    const int row = static_cast<int>(messages_.size());
    beginInsertRows({}, row, row);
    messages_.push_back(std::move(rec));
    if (static_cast<int>(messages_.size()) > max_messages_) {
        messages_.pop_front();
    }
    endInsertRows();
}

void MessageModel::AddSentMessage(const QVariantMap& payload, const QByteArray& raw_data)
{
    MessageRecord rec;
    rec.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
    rec.direction    = QStringLiteral("TX");
    rec.type         = payload.value("type", "Message").toString();
    rec.raw_data     = raw_data;
    rec.data         = QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact);

    const int row = static_cast<int>(messages_.size());
    beginInsertRows({}, row, row);
    messages_.push_back(std::move(rec));
    if (static_cast<int>(messages_.size()) > max_messages_) {
        messages_.pop_front();
    }
    endInsertRows();
}

void MessageModel::Clear()
{
    beginResetModel();
    messages_.clear();
    endResetModel();
}

} // namespace Application
} // namespace LinkForge
