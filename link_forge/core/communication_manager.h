#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <atomic>
#include <memory>

#include "iprotocol.h"
#include "itransport.h"

namespace LinkForge {
namespace Core {

/**
 * @brief Central communication manager -- the primary public API for the comms stack.
 *
 * Owns a Transport and a Protocol and runs all I/O on a dedicated worker thread,
 * so the owning (GUI) thread is never blocked by serial / network operations.
 *
 * Usage pattern:
 *   1. Call Initialize() to build transport + protocol.
 *   2. Call Connect() -- non-blocking; state arrives via StateChanged / ErrorOccurred.
 *   3. Call SendMessage() / SendRawData() -- fire-and-forget; DataSent confirms delivery.
 *   4. Received messages arrive via MessageReceived / RawDataReceived.
 *   5. Call Disconnect() or Reset() to tear down.
 *
 * Thread safety:
 *   All public methods are safe to call from the owning thread.
 *   The transport and protocol live in the private I/O thread and communicate
 *   with the owner only through Qt queued signals.
 */
class CommunicationManager : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    Q_ENUM(State)

    explicit CommunicationManager(QObject* parent = nullptr);
    ~CommunicationManager() override;

    /**
     * @brief Build and connect a transport + protocol pair.
     *
     * If a previous session exists it is torn down synchronously before the
     * new one is created.  Both objects are moved to the dedicated I/O thread.
     *
     * @return false if either factory lookup fails.
     */
    [[nodiscard]] bool Initialize(const QString& transport_type,
                                   const QString& protocol_type,
                                   const QVariantMap& transport_config,
                                   const QVariantMap& protocol_config = {});

    /// Non-blocking connect; result arrives via StateChanged / ErrorOccurred.
    void Connect();

    /// Non-blocking disconnect.
    void Disconnect();

    /**
     * @brief Encode payload with the active protocol and queue for async send.
     * @return false when not connected or protocol is absent.
     */
    [[nodiscard]] bool SendMessage(const QVariantMap& payload);

    /**
     * @brief Queue raw bytes for async send without protocol encoding.
     * @return false when not connected.
     */
    [[nodiscard]] bool SendRawData(const QByteArray& data);

    [[nodiscard]] State          GetState()     const noexcept { return state_.load(std::memory_order_acquire); }
    [[nodiscard]] bool           IsConnected()  const noexcept { return GetState() == State::Connected; }
    [[nodiscard]] ITransport::Ptr GetTransport() const          { return transport_; }
    [[nodiscard]] IProtocol::Ptr  GetProtocol()  const          { return protocol_; }

    /// Tear down the session, release transport/protocol and reset stats.
    void Reset();

signals:
    void StateChanged(LinkForge::Core::CommunicationManager::State new_state);
    void Connected();
    void Disconnected();
    void MessageReceived(const LinkForge::Core::IProtocol::Message& message);
    void RawDataReceived(const QByteArray& data);
    void DataSent(qint64 bytes);
    void ErrorOccurred(const QString& error);

private slots:
    void OnTransportStateChanged(ITransport::State transport_state);
    void OnTransportDataReceived(const QByteArray& data);
    void OnTransportError(const QString& error);
    void OnProtocolMessageParsed(const IProtocol::Message& message);
    void OnProtocolParseError(const QString& error);

private:
    void SetState(State new_state);
    void SetupConnections();
    void CleanupConnections();
    void StopIoThread();
    /// Post a raw send to the I/O thread (fire-and-forget).
    void DispatchSend(const QByteArray& data);

    ITransport::Ptr transport_;
    IProtocol::Ptr  protocol_;
    QThread         io_thread_;  ///< Dedicated event-loop thread for all I/O.

    std::atomic<State>   state_{State::Idle};
    std::atomic<quint64> tx_bytes_{0};
    std::atomic<quint64> rx_bytes_{0};

    QVariantMap transport_config_;
    QVariantMap protocol_config_;
};

} // namespace Core
} // namespace LinkForge
