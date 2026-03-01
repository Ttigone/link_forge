#include "communication_manager.h"
#include "transport_factory.h"
#include "protocol_factory.h"

#include <QDebug>
#include <QMetaObject>

namespace LinkForge {
namespace Core {

CommunicationManager::CommunicationManager(QObject* parent)
    : QObject(parent)
{
    qInfo() << "CommunicationManager: created";
}

CommunicationManager::~CommunicationManager()
{
    StopIoThread();
    qInfo() << "CommunicationManager: destroyed";
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool CommunicationManager::Initialize(const QString& transport_type,
                                       const QString& protocol_type,
                                       const QVariantMap& transport_config,
                                       const QVariantMap& protocol_config)
{
    // Tear down any previous session first (synchronous).
    StopIoThread();
    CleanupConnections();

    transport_config_ = transport_config;
    protocol_config_  = protocol_config;

    // Create objects without a Qt parent -- they will be moved to io_thread_.
    transport_ = TransportFactory::Create(transport_type, nullptr);
    if (!transport_) {
        emit ErrorOccurred(QString("Unknown transport type: %1").arg(transport_type));
        return false;
    }

    protocol_ = ProtocolFactory::Create(protocol_type, protocol_config, nullptr);
    if (!protocol_) {
        emit ErrorOccurred(QString("Unknown protocol type: %1").arg(protocol_type));
        transport_.reset();
        return false;
    }

    // Move both objects to the dedicated I/O thread.
    transport_->moveToThread(&io_thread_);
    protocol_->moveToThread(&io_thread_);

    SetupConnections();
    io_thread_.start();

    SetState(State::Idle);
    qInfo() << "CommunicationManager: initialized | transport:" << transport_type
            << "| protocol:" << protocol_type;
    return true;
}

void CommunicationManager::Connect()
{
    if (!transport_) {
        emit ErrorOccurred("Transport not initialised");
        return;
    }
    const auto s = GetState();
    if (s == State::Connected || s == State::Connecting) {
        return;
    }
    SetState(State::Connecting);

    // Post to I/O thread -- non-blocking for the caller.
    auto transport = transport_;
    auto config    = transport_config_;
    QMetaObject::invokeMethod(transport_.get(), [transport, config]() {
        transport->Connect(config);
    }, Qt::QueuedConnection);
}

void CommunicationManager::Disconnect()
{
    const auto s = GetState();
    if (s == State::Idle || s == State::Disconnecting) {
        return;
    }
    SetState(State::Disconnecting);

    auto transport = transport_;
    auto protocol  = protocol_;
    QMetaObject::invokeMethod(transport_.get(), [transport, protocol]() {
        transport->Disconnect();
        if (protocol) {
            protocol->Reset();
        }
    }, Qt::QueuedConnection);
}

bool CommunicationManager::SendMessage(const QVariantMap& payload)
{
    if (!IsConnected()) {
        emit ErrorOccurred("Cannot send message: not connected");
        return false;
    }
    if (!protocol_) {
        emit ErrorOccurred("Protocol not initialised");
        return false;
    }

    // Encode + send entirely on the I/O thread.
    auto protocol  = protocol_;
    auto transport = transport_;
    QMetaObject::invokeMethod(protocol_.get(),
        [this, protocol, transport, payload]() {
            auto opt = protocol->BuildMessage(payload);
            if (!opt.has_value()) {
                emit ErrorOccurred("Failed to encode message");
                return;
            }
            const qint64 sent = transport->Send(opt.value());
            if (sent > 0) {
                tx_bytes_.fetch_add(static_cast<quint64>(sent), std::memory_order_relaxed);
                emit DataSent(sent);
            } else {
                emit ErrorOccurred("Transport write failed");
            }
        }, Qt::QueuedConnection);

    return true;
}

bool CommunicationManager::SendRawData(const QByteArray& data)
{
    if (!IsConnected()) {
        emit ErrorOccurred("Cannot send data: not connected");
        return false;
    }
    DispatchSend(data);
    return true;
}

void CommunicationManager::Reset()
{
    StopIoThread();
    CleanupConnections();

    transport_.reset();
    protocol_.reset();

    tx_bytes_.store(0, std::memory_order_relaxed);
    rx_bytes_.store(0, std::memory_order_relaxed);

    SetState(State::Idle);
    qInfo() << "CommunicationManager: reset";
}

// ---------------------------------------------------------------------------
// Private slots (run in owner thread due to queued connections)
// ---------------------------------------------------------------------------

void CommunicationManager::OnTransportStateChanged(ITransport::State transport_state)
{
    switch (transport_state) {
        case ITransport::State::Connected:
            SetState(State::Connected);
            emit Connected();
            qInfo() << "CommunicationManager: transport connected";
            break;

        case ITransport::State::Disconnected:
            SetState(State::Idle);
            emit Disconnected();
            qInfo() << "CommunicationManager: transport disconnected";
            break;

        case ITransport::State::Error:
            SetState(State::Error);
            break;

        default:
            break;
    }
}

void CommunicationManager::OnTransportDataReceived(const QByteArray& data)
{
    rx_bytes_.fetch_add(static_cast<quint64>(data.size()), std::memory_order_relaxed);
    emit RawDataReceived(data);

    if (protocol_) {
        // Forward parsing to the I/O thread.
        auto protocol = protocol_;
        QMetaObject::invokeMethod(protocol_.get(), [protocol, data]() {
            protocol->ParseData(data);
        }, Qt::QueuedConnection);
    }
}

void CommunicationManager::OnTransportError(const QString& error)
{
    emit ErrorOccurred(QString("Transport: %1").arg(error));
}

void CommunicationManager::OnProtocolMessageParsed(const IProtocol::Message& message)
{
    emit MessageReceived(message);
}

void CommunicationManager::OnProtocolParseError(const QString& error)
{
    emit ErrorOccurred(QString("Protocol: %1").arg(error));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void CommunicationManager::SetState(State new_state)
{
    const State old = state_.exchange(new_state, std::memory_order_acq_rel);
    if (old != new_state) {
        emit StateChanged(new_state);
    }
}

void CommunicationManager::SetupConnections()
{
    if (!transport_ || !protocol_) {
        return;
    }

    // Transport -> manager  (Qt::AutoConnection -> queued across threads).
    QObject::connect(transport_.get(), &ITransport::StateChanged,
                     this, &CommunicationManager::OnTransportStateChanged);
    QObject::connect(transport_.get(), &ITransport::DataReceived,
                     this, &CommunicationManager::OnTransportDataReceived);
    QObject::connect(transport_.get(), &ITransport::ErrorOccurred,
                     this, &CommunicationManager::OnTransportError);

    // Protocol -> manager.
    QObject::connect(protocol_.get(), &IProtocol::MessageParsed,
                     this, &CommunicationManager::OnProtocolMessageParsed);
    QObject::connect(protocol_.get(), &IProtocol::ParseError,
                     this, &CommunicationManager::OnProtocolParseError);
}

void CommunicationManager::CleanupConnections()
{
    if (transport_) {
        QObject::disconnect(transport_.get(), nullptr, this, nullptr);
    }
    if (protocol_) {
        QObject::disconnect(protocol_.get(), nullptr, this, nullptr);
    }
}

void CommunicationManager::StopIoThread()
{
    if (!io_thread_.isRunning()) {
        return;
    }

    // Ask the transport to disconnect from within its own thread, then wait.
    if (transport_) {
        QMetaObject::invokeMethod(transport_.get(), [t = transport_.get()]() {
            t->Disconnect();
        }, Qt::BlockingQueuedConnection);
    }

    io_thread_.quit();
    if (!io_thread_.wait(5000)) {
        qWarning() << "CommunicationManager: I/O thread did not stop in 5 s -- forcing";
        io_thread_.terminate();
        io_thread_.wait();
    }
}

void CommunicationManager::DispatchSend(const QByteArray& data)
{
    auto transport = transport_;
    QMetaObject::invokeMethod(transport_.get(),
        [this, transport, data]() {
            const qint64 sent = transport->Send(data);
            if (sent > 0) {
                tx_bytes_.fetch_add(static_cast<quint64>(sent), std::memory_order_relaxed);
                emit DataSent(sent);
            } else if (sent < 0) {
                emit ErrorOccurred("Raw send failed");
            }
        }, Qt::QueuedConnection);
}

} // namespace Core
} // namespace LinkForge
