#include "can_transport.h"
#include "../core/type_names.h"

#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QDebug>
#include <QDataStream>
#include <QIODevice>

namespace LinkForge {
namespace Transport {

using namespace Core;

CanTransport::CanTransport(QObject* parent)
    : ITransport(parent)
{}

CanTransport::~CanTransport()
{
    Disconnect();
}

// ────────────────────────────────────────────────────────────────────────────
// ITransport -- Connect / Disconnect / Send
// ────────────────────────────────────────────────────────────────────────────

bool CanTransport::Connect(const QVariantMap& config)
{
    if (state_.load() == State::Connected) {
        qWarning() << "CanTransport: already connected";
        return true;
    }

    config_ = config;

    const QString plugin    = config_.value(TypeNames::Can::kPlugin,
                                             QStringLiteral("socketcan")).toString();
    const QString interface = config_.value(TypeNames::Can::kInterface,
                                             QStringLiteral("can0")).toString();
    can_fd_  = config_.value(TypeNames::Can::kFd, false).toBool();

    SetState(State::Connecting);

    QString error_str;
    device_.reset(QCanBus::instance()->createDevice(plugin, interface, &error_str));

    if (!device_) {
        qWarning() << "CanTransport: createDevice failed:" << error_str;
        emit ErrorOccurred(error_str);
        SetState(State::Error);
        return false;
    }

    // Wire device signals before calling connectDevice().
    QObject::connect(device_.get(), &QCanBusDevice::framesReceived,
                     this, &CanTransport::OnFramesReceived);
    QObject::connect(device_.get(), &QCanBusDevice::errorOccurred,
                     this, &CanTransport::OnErrorOccurred);
    QObject::connect(device_.get(), &QCanBusDevice::stateChanged,
                     this, &CanTransport::OnStateChanged);

    if (!ConfigureDevice()) {
        device_.reset();
        SetState(State::Error);
        return false;
    }

    if (!device_->connectDevice()) {
        const QString err = device_->errorString();
        qWarning() << "CanTransport: connectDevice failed:" << err;
        emit ErrorOccurred(err);
        device_.reset();
        SetState(State::Error);
        return false;
    }

    // stateChanged signal will drive SetState(Connected).
    return true;
}

void CanTransport::Disconnect()
{
    if (device_) {
        device_->disconnectDevice();
        QObject::disconnect(device_.get(), nullptr, this, nullptr);
        device_.reset();
    }
    SetState(State::Disconnected);
    emit Disconnected();
}

qint64 CanTransport::Send(const QByteArray& data)
{
    if (state_.load() != State::Connected || !device_) {
        emit ErrorOccurred(QStringLiteral("CanTransport: not connected"));
        return -1;
    }

    const QCanBusFrame frame = DeserialiseFrame(data);
    if (!frame.isValid()) {
        const QString err = QStringLiteral("CanTransport: invalid frame bytes");
        emit ErrorOccurred(err);
        qWarning() << err;
        return -1;
    }

    if (!device_->writeFrame(frame)) {
        const QString err = device_->errorString();
        emit ErrorOccurred(err);
        qWarning() << "CanTransport: writeFrame failed:" << err;
        return -1;
    }
    return data.size();
}

// ────────────────────────────────────────────────────────────────────────────
// ITransport -- Queries
// ────────────────────────────────────────────────────────────────────────────

ITransport::State CanTransport::GetState() const
{
    return state_.load();
}

ITransport::Type CanTransport::GetType() const
{
    return Type::Can;
}

QString CanTransport::GetName() const
{
    return QString::fromUtf8(TypeNames::Transport::kCan);
}

QVariantMap CanTransport::GetConfig() const
{
    return config_;
}

// ────────────────────────────────────────────────────────────────────────────
// Static helpers -- serialise / deserialise CAN frames
// ────────────────────────────────────────────────────────────────────────────
// Wire format:
//   [0-3] frame ID (big-endian uint32)
//   [4]   flags: bit0=EFF, bit1=RTR, bit2=FD
//   [5]   DLC (payload byte count)
//   [6..] payload bytes
// ────────────────────────────────────────────────────────────────────────────

QByteArray CanTransport::SerialiseFrame(const QCanBusFrame& frame)
{
    QByteArray buf;
    buf.reserve(6 + frame.payload().size());

    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    ds << static_cast<quint32>(frame.frameId());

    quint8 flags = 0;
    if (frame.hasExtendedFrameFormat()) flags |= 0x01;
    if (frame.frameType() == QCanBusFrame::RemoteRequestFrame) flags |= 0x02;
    if (frame.frameType() == QCanBusFrame::DataFrame && frame.hasBitrateSwitch()) flags |= 0x04;

    ds << flags;
    ds << static_cast<quint8>(frame.payload().size());

    buf.append(frame.payload());
    return buf;
}

QCanBusFrame CanTransport::DeserialiseFrame(const QByteArray& data)
{
    if (data.size() < 6) {
        return QCanBusFrame(QCanBusFrame::InvalidFrame);
    }

    QDataStream ds(data);
    ds.setByteOrder(QDataStream::BigEndian);

    quint32 frame_id{};
    quint8  flags{};
    quint8  dlc{};

    ds >> frame_id >> flags >> dlc;

    const bool has_eff = (flags & 0x01) != 0;
    const bool is_rtr  = (flags & 0x02) != 0;

    const QByteArray payload = data.mid(6, dlc);

    QCanBusFrame frame;
    frame.setFrameId(frame_id);
    frame.setExtendedFrameFormat(has_eff);
    frame.setPayload(payload);
    frame.setFrameType(is_rtr ? QCanBusFrame::RemoteRequestFrame
                               : QCanBusFrame::DataFrame);
    return frame;
}

// ────────────────────────────────────────────────────────────────────────────
// Private slots
// ────────────────────────────────────────────────────────────────────────────

void CanTransport::OnFramesReceived()
{
    if (!device_) return;

    while (device_->framesAvailable() > 0) {
        const QCanBusFrame frame = device_->readFrame();
        emit DataReceived(SerialiseFrame(frame));
    }
}

void CanTransport::OnErrorOccurred(QCanBusDevice::CanBusError error)
{
    if (error == QCanBusDevice::NoError) return;

    const QString msg = device_ ? device_->errorString()
                                 : QStringLiteral("Unknown CAN error");
    qWarning() << "CanTransport error:" << msg;
    emit ErrorOccurred(msg);
    SetState(State::Error);
}

void CanTransport::OnStateChanged(QCanBusDevice::CanBusDeviceState device_state)
{
    switch (device_state) {
    case QCanBusDevice::ConnectedState:
        SetState(State::Connected);
        emit Connected();
        break;
    case QCanBusDevice::UnconnectedState:
        SetState(State::Disconnected);
        emit Disconnected();
        break;
    default:
        break;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Private helpers
// ────────────────────────────────────────────────────────────────────────────

void CanTransport::SetState(State new_state)
{
    state_.store(new_state);
    emit StateChanged(new_state);
}

bool CanTransport::ConfigureDevice()
{
    if (!device_) return false;

    const quint32 bitrate = config_.value(TypeNames::Can::kBitrate, 500000u).toUInt();
    if (bitrate > 0) {
        device_->setConfigurationParameter(QCanBusDevice::BitRateKey,
                                            QVariant(bitrate));
    }

    const bool loopback = config_.value(TypeNames::Can::kLoopback, false).toBool();
    device_->setConfigurationParameter(QCanBusDevice::LoopbackKey,
                                        QVariant(loopback));

    if (can_fd_) {
        device_->setConfigurationParameter(QCanBusDevice::CanFdKey,
                                            QVariant(true));
    }
    return true;
}

} // namespace Transport
} // namespace LinkForge
