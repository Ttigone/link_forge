#pragma once

#include <QObject>
#include <memory>

#include "../core/communication_manager.h"
#include "message_model.h"

namespace LinkForge {
namespace Application {

/**
 * @brief Main application controller.
 *
 * Bridges the UI layer and the communication stack.
 * Owns a CommunicationManager and a MessageModel.
 */
class MainController : public QObject {
    Q_OBJECT

public:
    explicit MainController(QObject* parent = nullptr);
    ~MainController() override;

    [[nodiscard]] bool InitializeCommunication(
        const QString& transport_type,
        const QString& protocol_type,
        const QVariantMap& transport_config,
        const QVariantMap& protocol_config = {});

    /// Non-blocking connect (result via ConnectionStateChanged signal).
    void Connect();
    void Disconnect();

    [[nodiscard]] bool  SendMessage(const QVariantMap& payload);
    [[nodiscard]] bool  SendRawData(const QByteArray& data);

    [[nodiscard]] MessageModel*                 GetMessageModel()       { return message_model_.get(); }
    [[nodiscard]] Core::CommunicationManager*   GetCommManager()        { return comm_manager_.get(); }
    [[nodiscard]] bool                          IsConnected() const;
    [[nodiscard]] QVariantMap                   GetStatistics() const;

signals:
    void ConnectionStateChanged(bool connected);
    void ErrorOccurred(const QString& error);
    void MessageReceived(const QString& message_type);
    void StatisticsUpdated(const QVariantMap& stats);

private slots:
    void OnCommStateChanged(Core::CommunicationManager::State state);
    void OnCommMessageReceived(const Core::IProtocol::Message& message);
    void OnCommErrorOccurred(const QString& error);
    void OnCommDataSent(qint64 bytes);

private:
    void UpdateStatistics();

    std::unique_ptr<Core::CommunicationManager> comm_manager_;
    std::unique_ptr<MessageModel>               message_model_;

    quint64 total_bytes_sent_{0};
    quint64 total_bytes_received_{0};
    quint64 total_messages_sent_{0};
    quint64 total_messages_received_{0};
};

} // namespace Application
} // namespace LinkForge
