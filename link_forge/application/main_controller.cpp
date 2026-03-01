#include "main_controller.h"

#include <QDebug>
#include <unordered_map>

namespace LinkForge {
namespace Application {

MainController::MainController(QObject* parent)
    : QObject(parent)
    , comm_manager_(std::make_unique<Core::CommunicationManager>(this))
    , message_model_(std::make_unique<MessageModel>(this))
{
    QObject::connect(comm_manager_.get(), &Core::CommunicationManager::StateChanged,
                     this, &MainController::OnCommStateChanged);
    QObject::connect(comm_manager_.get(), &Core::CommunicationManager::MessageReceived,
                     this, &MainController::OnCommMessageReceived);
    QObject::connect(comm_manager_.get(), &Core::CommunicationManager::ErrorOccurred,
                     this, &MainController::OnCommErrorOccurred);
    QObject::connect(comm_manager_.get(), &Core::CommunicationManager::DataSent,
                     this, &MainController::OnCommDataSent);

    qInfo() << "MainController: created";
}

MainController::~MainController()
{
    Disconnect();
    qInfo() << "MainController: destroyed";
}

bool MainController::InitializeCommunication(const QString& transport_type,
                                              const QString& protocol_type,
                                              const QVariantMap& transport_config,
                                              const QVariantMap& protocol_config)
{
    return comm_manager_->Initialize(transport_type, protocol_type,
                                     transport_config, protocol_config);
}

void MainController::Connect()
{
    comm_manager_->Connect();
}

void MainController::Disconnect()
{
    comm_manager_->Disconnect();
}

bool MainController::SendMessage(const QVariantMap& payload)
{
    if (!comm_manager_->SendMessage(payload)) {
        return false;
    }
    // Optimistically record the outgoing message in the model using a rebuild.
    if (const auto proto = comm_manager_->GetProtocol()) {
        if (auto opt = proto->BuildMessage(payload)) {
            message_model_->AddSentMessage(payload, opt.value());
            ++total_messages_sent_;
            UpdateStatistics();
        }
    }
    return true;
}

bool MainController::SendRawData(const QByteArray& data)
{
    if (!comm_manager_->SendRawData(data)) {
        return false;
    }
    QVariantMap meta;
    meta["type"] = QStringLiteral("RawData");
    meta["size"] = data.size();
    message_model_->AddSentMessage(meta, data);
    ++total_messages_sent_;
    UpdateStatistics();
    return true;
}

bool MainController::IsConnected() const
{
    return comm_manager_->IsConnected();
}

QVariantMap MainController::GetStatistics() const
{
    QVariantMap stats;
    stats["bytesSent"]         = total_bytes_sent_;
    stats["bytesReceived"]     = total_bytes_received_;
    stats["messagesSent"]      = total_messages_sent_;
    stats["messagesReceived"]  = total_messages_received_;
    return stats;
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void MainController::OnCommStateChanged(Core::CommunicationManager::State state)
{
    const bool connected = (state == Core::CommunicationManager::State::Connected);
    emit ConnectionStateChanged(connected);

    // Use a lookup instead of a chained switch for state-to-string.
    static const std::unordered_map<Core::CommunicationManager::State, const char*> kStateNames = {
        {Core::CommunicationManager::State::Idle,          "Idle"},
        {Core::CommunicationManager::State::Connecting,    "Connecting"},
        {Core::CommunicationManager::State::Connected,     "Connected"},
        {Core::CommunicationManager::State::Disconnecting, "Disconnecting"},
        {Core::CommunicationManager::State::Error,         "Error"},
    };
    auto it = kStateNames.find(state);
    qInfo() << "MainController: state ->" << (it != kStateNames.end() ? it->second : "Unknown");
}

void MainController::OnCommMessageReceived(const Core::IProtocol::Message& message)
{
    message_model_->AddReceivedMessage(message);
    ++total_messages_received_;
    total_bytes_received_ += static_cast<quint64>(message.raw_data.size());
    emit MessageReceived(message.message_type);
    UpdateStatistics();
}

void MainController::OnCommErrorOccurred(const QString& error)
{
    qWarning() << "MainController: communication error:" << error;
    emit ErrorOccurred(error);
}

void MainController::OnCommDataSent(qint64 bytes)
{
    total_bytes_sent_ += static_cast<quint64>(bytes);
    UpdateStatistics();
}

void MainController::UpdateStatistics()
{
    emit StatisticsUpdated(GetStatistics());
}

} // namespace Application
} // namespace LinkForge
