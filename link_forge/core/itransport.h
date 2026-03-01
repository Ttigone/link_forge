#pragma once

#include <QByteArray>
#include <QObject>
#include <QVariantMap>
#include <memory>

namespace LinkForge {
namespace Core {

/**
 * @brief Transport layer abstract interface.
 *
 * Defines the unified contract for all transports (Serial, TCP, UDP, CAN, etc.).
 *
 * Network transports (TCP, UDP) are non-blocking: Connect() returns true
 * immediately when the connection attempt was started.  The outcome arrives
 * via the Connected() / ErrorOccurred() signals.
 * Serial transports open synchronously and return the result from Connect().
 */
class ITransport : public QObject {
    Q_OBJECT

public:
    using Ptr     = std::shared_ptr<ITransport>;
    using WeakPtr = std::weak_ptr<ITransport>;

    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    Q_ENUM(State)

    enum class Type {
        Serial,
        TcpSocket,
        UdpSocket,
        Can,
        Unknown
    };
    Q_ENUM(Type)

    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    ITransport(const ITransport&)            = delete;
    ITransport& operator=(const ITransport&) = delete;

    /**
     * @brief Initiate connection using the supplied config map.
     * @return false only if the attempt could not be started at all.
     */
    virtual bool Connect(const QVariantMap& config) = 0;

    /// Gracefully terminate the connection.
    virtual void Disconnect() = 0;

    /**
     * @brief Write bytes to the transport.
     * @return Bytes queued/written (>= 0), or -1 on hard error.
     */
    [[nodiscard]] virtual qint64       Send(const QByteArray& data) = 0;

    [[nodiscard]] virtual State        GetState()  const = 0;
    [[nodiscard]] virtual Type         GetType()   const = 0;
    [[nodiscard]] virtual QVariantMap  GetConfig() const = 0;

    [[nodiscard]] virtual bool IsConnected() const { return GetState() == State::Connected; }

signals:
    void DataReceived(const QByteArray& data);
    void StateChanged(LinkForge::Core::ITransport::State new_state);
    void ErrorOccurred(const QString& error);
    void Connected();
    void Disconnected();
};

} // namespace Core
} // namespace LinkForge
