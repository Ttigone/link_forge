#include "serial_transport.h"
#include "../core/type_names.h"

#include <QSerialPortInfo>
#include <QDebug>
#include <unordered_map>

namespace LinkForge {
namespace Transport {

SerialTransport::SerialTransport(QObject* parent)
    : ITransport(parent)
    , port_(std::make_unique<QSerialPort>(this))
{
    QObject::connect(port_.get(), &QSerialPort::readyRead,
                     this, &SerialTransport::OnReadyRead);
    QObject::connect(port_.get(), &QSerialPort::errorOccurred,
                     this, &SerialTransport::OnErrorOccurred);
    QObject::connect(port_.get(), &QSerialPort::bytesWritten,
                     this, &SerialTransport::OnBytesWritten);
}

SerialTransport::~SerialTransport()
{
    if (port_ && port_->isOpen()) {
        port_->close();
    }
}

bool SerialTransport::Connect(const QVariantMap& config)
{
    if (state_ == State::Connected) {
        qWarning() << "SerialTransport: already connected";
        return true;
    }

    SetState(State::Connecting);
    config_ = config;
    if (!ConfigurePort(config)) {
        SetState(State::Error);
        return false;
    }
    if (port_->open(QIODevice::ReadWrite)) {
        SetState(State::Connected);
        emit Connected();
        qInfo() << "SerialTransport: opened" << port_->portName();
        return true;
    }

    const QString err = QString("Failed to open %1: %2")
                        .arg(port_->portName(), port_->errorString());
    emit ErrorOccurred(err);
    SetState(State::Error);
    return false;
}

void SerialTransport::Disconnect()
{
    if (state_ == State::Disconnected) {
        return;
    }
    SetState(State::Disconnecting);

    if (port_ && port_->isOpen()) {
        port_->close();
    }

    read_buffer_.clear();
    SetState(State::Disconnected);
    emit Disconnected();
    qInfo() << "SerialTransport: closed";
}

qint64 SerialTransport::Send(const QByteArray& data)
{
    if (state_ != State::Connected) {
        qWarning() << "SerialTransport::Send: not connected";
        return -1;
    }
    const qint64 written = port_->write(data);
    if (written < 0) {
        emit ErrorOccurred(QString("Write failed: %1").arg(port_->errorString()));
        return -1;
    }
    port_->flush();
    return written;
}

QStringList SerialTransport::AvailablePorts()
{
    QStringList ports;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        ports.append(info.portName());
    }
    return ports;
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void SerialTransport::OnReadyRead()
{
    if (!port_ || !port_->isOpen()) {
        return;
    }
    const QByteArray data = port_->readAll();
    if (!data.isEmpty()) {
        read_buffer_.append(data);
        emit DataReceived(data);
    }
}

void SerialTransport::OnErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    emit ErrorOccurred(QString("Serial error: %1").arg(port_->errorString()));

    if (error == QSerialPort::ResourceError  ||
        error == QSerialPort::PermissionError ||
        error == QSerialPort::DeviceNotFoundError)
    {
        SetState(State::Error);
        Disconnect();
    }
}

void SerialTransport::OnBytesWritten(qint64 /*bytes*/) {}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SerialTransport::SetState(State new_state)
{
    if (state_ != new_state) {
        state_ = new_state;
        emit StateChanged(new_state);
    }
}

bool SerialTransport::ConfigurePort(const QVariantMap& config)
{
    using namespace Core::TypeNames;

    // Port name
    const QString port_name = config.value(QString(Config::kPort), "COM1").toString();
    port_->setPortName(port_name);

    // Baud rate
    const int baud_rate = config.value(QString(Config::kBaudRate), 9600).toInt();
    if (!port_->setBaudRate(baud_rate)) {
        emit ErrorOccurred(QString("Invalid baud rate: %1").arg(baud_rate));
        return false;
    }

    // Data bits -- switch over integer avoids string comparison
    static const std::unordered_map<int, QSerialPort::DataBits> kDataBitsMap = {
        {5, QSerialPort::Data5}, {6, QSerialPort::Data6},
        {7, QSerialPort::Data7}, {8, QSerialPort::Data8},
    };
    const int data_bits_val = config.value(QString(Config::kDataBits), 8).toInt();
    auto db_it = kDataBitsMap.find(data_bits_val);
    if (db_it == kDataBitsMap.end()) {
        emit ErrorOccurred(QString("Invalid data bits: %1").arg(data_bits_val));
        return false;
    }
    port_->setDataBits(db_it->second);

    // Parity
    static const std::unordered_map<QString, QSerialPort::Parity> kParityMap = {
        {QString(Serial::kParityNone),  QSerialPort::NoParity},
        {QString(Serial::kParityEven),  QSerialPort::EvenParity},
        {QString(Serial::kParityOdd),   QSerialPort::OddParity},
        {QString(Serial::kParitySpace), QSerialPort::SpaceParity},
        {QString(Serial::kParityMark),  QSerialPort::MarkParity},
    };
    const QString parity_str = config.value(QString(Config::kParity),
                                             QString(Serial::kParityNone)).toString();
    auto par_it = kParityMap.find(parity_str);
    if (par_it == kParityMap.end()) {
        emit ErrorOccurred(QString("Invalid parity: %1").arg(parity_str));
        return false;
    }
    port_->setParity(par_it->second);

    // Stop bits
    static const std::unordered_map<QString, QSerialPort::StopBits> kStopBitsMap = {
        {QString(Serial::kStopBits1),   QSerialPort::OneStop},
        {QString(Serial::kStopBits1_5), QSerialPort::OneAndHalfStop},
        {QString(Serial::kStopBits2),   QSerialPort::TwoStop},
    };
    const QString stop_str = config.value(QString(Config::kStopBits),
                                           QString(Serial::kStopBits1)).toString();
    auto sb_it = kStopBitsMap.find(stop_str);
    if (sb_it == kStopBitsMap.end()) {
        emit ErrorOccurred(QString("Invalid stop bits: %1").arg(stop_str));
        return false;
    }
    port_->setStopBits(sb_it->second);

    // Flow control
    static const std::unordered_map<QString, QSerialPort::FlowControl> kFlowMap = {
        {QString(Serial::kFlowNone),     QSerialPort::NoFlowControl},
        {QString(Serial::kFlowHardware), QSerialPort::HardwareControl},
        {QString(Serial::kFlowSoftware), QSerialPort::SoftwareControl},
    };
    const QString flow_str = config.value(QString(Config::kFlowControl),
                                           QString(Serial::kFlowNone)).toString();
    auto fc_it = kFlowMap.find(flow_str);
    if (fc_it == kFlowMap.end()) {
        emit ErrorOccurred(QString("Invalid flow control: %1").arg(flow_str));
        return false;
    }
    port_->setFlowControl(fc_it->second);

    return true;
}

} // namespace Transport
} // namespace LinkForge
