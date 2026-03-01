#pragma once

#include <QHostAddress>
#include <QTcpSocket>
#include <QUdpSocket>
#include <memory>

#include "../core/itransport.h"

namespace LinkForge {
namespace Transport {

// ============================================================================
// TCP
// ============================================================================

/**
 * @brief TCP socket transport (non-blocking connect).
 *
 * Connect() calls connectToHost() and returns immediately.
 * The Connected() / ErrorOccurred() signals deliver the outcome.
 */
class TcpSocketTransport : public Core::ITransport {
    Q_OBJECT

public:
    explicit TcpSocketTransport(QObject* parent = nullptr);
    ~TcpSocketTransport() override;

    bool           Connect(const QVariantMap& config)  override;
    void           Disconnect()                         override;
    [[nodiscard]] qint64       Send(const QByteArray& data)    override;
    [[nodiscard]] State        GetState()  const override { return state_; }
    [[nodiscard]] Type         GetType()   const override { return Type::TcpSocket; }
    [[nodiscard]] QVariantMap  GetConfig() const override { return config_; }

private slots:
    void OnConnected();
    void OnDisconnected();
    void OnReadyRead();
    void OnErrorOccurred(QAbstractSocket::SocketError error);

private:
    void SetState(State new_state);

    std::unique_ptr<QTcpSocket> socket_;
    State       state_{State::Disconnected};
    QVariantMap config_;
};

// ============================================================================
// UDP
// ============================================================================

/**
 * @brief UDP socket transport.
 *
 * Binds a local endpoint and sends datagrams to a configurable remote address.
 */
class UdpSocketTransport : public Core::ITransport {
    Q_OBJECT

public:
    explicit UdpSocketTransport(QObject* parent = nullptr);
    ~UdpSocketTransport() override;

    bool           Connect(const QVariantMap& config)  override;
    void           Disconnect()                         override;
    [[nodiscard]] qint64       Send(const QByteArray& data)    override;
    [[nodiscard]] State        GetState()  const override { return state_; }
    [[nodiscard]] Type         GetType()   const override { return Type::UdpSocket; }
    [[nodiscard]] QVariantMap  GetConfig() const override { return config_; }

private slots:
    void OnReadyRead();
    void OnErrorOccurred(QAbstractSocket::SocketError error);

private:
    void SetState(State new_state);

    std::unique_ptr<QUdpSocket> socket_;
    State        state_{State::Disconnected};
    QVariantMap  config_;
    QHostAddress remote_host_;
    quint16      remote_port_{0};
};

} // namespace Transport
} // namespace LinkForge
