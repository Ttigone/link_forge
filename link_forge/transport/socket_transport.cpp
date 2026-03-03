#include "socket_transport.h"
#include "../core/type_names.h"

#include <QDebug>

namespace LinkForge {
namespace Transport {

// ============================================================================
// TcpSocketTransport
// ============================================================================

TcpSocketTransport::TcpSocketTransport(QObject* parent)
    : ITransport(parent)
    , socket_(std::make_unique<QTcpSocket>(this))
{
    QObject::connect(socket_.get(), &QTcpSocket::connected,
                     this, &TcpSocketTransport::OnConnected);
    QObject::connect(socket_.get(), &QTcpSocket::disconnected,
                     this, &TcpSocketTransport::OnDisconnected);
    QObject::connect(socket_.get(), &QTcpSocket::readyRead,
                     this, &TcpSocketTransport::OnReadyRead);
    QObject::connect(socket_.get(), &QTcpSocket::errorOccurred,
                     this, &TcpSocketTransport::OnErrorOccurred);
}

TcpSocketTransport::~TcpSocketTransport()
{
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
}

bool TcpSocketTransport::Connect(const QVariantMap& config)
{
    if (state_ == State::Connected) {
        qWarning() << "TcpSocketTransport: already connected";
        return true;
    }
    config_ = config;

    const QString host = config.value(QString::fromLatin1(Core::TypeNames::Config::kHost.data()), "127.0.0.1").toString();
    const quint16 port = static_cast<quint16>(
        config.value(QString::fromLatin1(Core::TypeNames::Config::kPort.data()), 8080).toUInt());

    SetState(State::Connecting);
    // Non-blocking -- result arrives via OnConnected / OnErrorOccurred.
    socket_->connectToHost(host, port);
    return true;
}

void TcpSocketTransport::Disconnect()
{
    if (state_ == State::Disconnected) {
        return;
    }
    SetState(State::Disconnecting);
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

qint64 TcpSocketTransport::Send(const QByteArray& data)
{
    if (state_ != State::Connected) {
        qWarning() << "TcpSocketTransport::Send: not connected";
        return -1;
    }
    const qint64 written = socket_->write(data);
    if (written < 0) {
        emit ErrorOccurred(QString("TCP write failed: %1").arg(socket_->errorString()));
        return -1;
    }
    socket_->flush();
    return written;
}

void TcpSocketTransport::OnConnected()
{
    SetState(State::Connected);
    emit Connected();
    qInfo() << "TcpSocketTransport: connected to"
            << socket_->peerAddress().toString() << ":" << socket_->peerPort();
}

void TcpSocketTransport::OnDisconnected()
{
    SetState(State::Disconnected);
    emit Disconnected();
    qInfo() << "TcpSocketTransport: disconnected";
}

void TcpSocketTransport::OnReadyRead()
{
    if (state_ != State::Connected) {
        return;
    }
    const QByteArray data = socket_->readAll();
    if (!data.isEmpty()) {
        emit DataReceived(data);
    }
}

void TcpSocketTransport::OnErrorOccurred(QAbstractSocket::SocketError /*error*/)
{
    emit ErrorOccurred(QString("TCP error: %1").arg(socket_->errorString()));
    SetState(State::Error);
}

void TcpSocketTransport::SetState(State new_state)
{
    if (state_ != new_state) {
        state_ = new_state;
        emit StateChanged(new_state);
    }
}

// ============================================================================
// UdpSocketTransport
// ============================================================================

UdpSocketTransport::UdpSocketTransport(QObject* parent)
    : ITransport(parent)
    , socket_(std::make_unique<QUdpSocket>(this))
{
    QObject::connect(socket_.get(), &QUdpSocket::readyRead,
                     this, &UdpSocketTransport::OnReadyRead);
    QObject::connect(socket_.get(), &QUdpSocket::errorOccurred,
                     this, &UdpSocketTransport::OnErrorOccurred);
}

UdpSocketTransport::~UdpSocketTransport()
{
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
}

bool UdpSocketTransport::Connect(const QVariantMap& config)
{
    if (state_ == State::Connected) {
        qWarning() << "UdpSocketTransport: already connected";
        return true;
    }
    config_ = config;

    const QString local_host = config.value(QString::fromLatin1(Core::TypeNames::Config::kLocalHost.data()), "0.0.0.0").toString();
    const quint16 local_port = static_cast<quint16>(
        config.value(QString::fromLatin1(Core::TypeNames::Config::kLocalPort.data()), 0).toUInt());

    remote_host_ = QHostAddress(config.value(QString::fromLatin1(Core::TypeNames::Config::kRemoteHost.data()), "127.0.0.1").toString());
    remote_port_ = static_cast<quint16>(
        config.value(QString::fromLatin1(Core::TypeNames::Config::kRemotePort.data()), 8080).toUInt());

    SetState(State::Connecting);

    if (socket_->bind(QHostAddress(local_host), local_port)) {
        SetState(State::Connected);
        emit Connected();
        qInfo() << "UdpSocketTransport: bound to" << local_host << ":" << local_port;
        return true;
    }

    const QString err = QString("UDP bind failed: %1").arg(socket_->errorString());
    emit ErrorOccurred(err);
    SetState(State::Error);
    return false;
}

void UdpSocketTransport::Disconnect()
{
    if (state_ == State::Disconnected) {
        return;
    }
    SetState(State::Disconnecting);
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
    SetState(State::Disconnected);
    emit Disconnected();
    qInfo() << "UdpSocketTransport: closed";
}

qint64 UdpSocketTransport::Send(const QByteArray& data)
{
    if (state_ != State::Connected) {
        qWarning() << "UdpSocketTransport::Send: not connected";
        return -1;
    }
    const qint64 written = socket_->writeDatagram(data, remote_host_, remote_port_);
    if (written < 0) {
        emit ErrorOccurred(QString("UDP write failed: %1").arg(socket_->errorString()));
        return -1;
    }
    return written;
}

void UdpSocketTransport::OnReadyRead()
{
    if (state_ != State::Connected) {
        return;
    }
    while (socket_->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(socket_->pendingDatagramSize()));
        socket_->readDatagram(datagram.data(), datagram.size());
        if (!datagram.isEmpty()) {
            emit DataReceived(datagram);
        }
    }
}

void UdpSocketTransport::OnErrorOccurred(QAbstractSocket::SocketError /*error*/)
{
    emit ErrorOccurred(QString("UDP error: %1").arg(socket_->errorString()));
    SetState(State::Error);
}

void UdpSocketTransport::SetState(State new_state)
{
    if (state_ != new_state) {
        state_ = new_state;
        emit StateChanged(new_state);
    }
}

} // namespace Transport
} // namespace LinkForge
