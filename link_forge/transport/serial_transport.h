#pragma once

#include <QSerialPort>
#include <memory>

#include "../core/itransport.h"

namespace LinkForge {
namespace Transport {

/**
 * @brief Serial port transport.
 *
 * Synchronous open/close; asynchronous (signal-driven) read path.
 * Intended to run on the CommunicationManager I/O thread.
 */
class SerialTransport : public Core::ITransport {
    Q_OBJECT

public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    bool           Connect(const QVariantMap& config)  override;
    void           Disconnect()                         override;
    [[nodiscard]] qint64       Send(const QByteArray& data)    override;
    [[nodiscard]] State        GetState()  const override { return state_; }
    [[nodiscard]] Type         GetType()   const override { return Type::Serial; }
    [[nodiscard]] QVariantMap  GetConfig() const override { return config_; }

    /// Returns a list of available port names on the current system.
    [[nodiscard]] static QStringList AvailablePorts();

private slots:
    void OnReadyRead();
    void OnErrorOccurred(QSerialPort::SerialPortError error);
    void OnBytesWritten(qint64 bytes);

private:
    void SetState(State new_state);
    [[nodiscard]] bool ConfigurePort(const QVariantMap& config);

    std::unique_ptr<QSerialPort> port_;
    State       state_{State::Disconnected};
    QVariantMap config_;
    QByteArray  read_buffer_;
};

} // namespace Transport
} // namespace LinkForge
