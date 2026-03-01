#pragma once

#include <QObject>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <memory>
#include <atomic>

#include "../core/itransport.h"

namespace LinkForge {
namespace Transport {

/**
 * @brief CAN bus transport (Qt SerialBus / QCanBusDevice).
 *
 * Wraps any CAN backend provided by Qt (socketcan, peakcan, vectorcan, etc.)
 * behind the uniform ITransport interface.
 *
 * Configuration keys (see TypeNames::Can namespace):
 *   "canPlugin"    - backend plugin name, e.g. "socketcan", "peakcan", "vectorcan"
 *   "canInterface" - device interface name, e.g. "can0" or "PCAN_USBBUS1"
 *   "canBitrate"   - bit rate in bps (default 500000), ignored on virtual CAN
 *   "canFd"        - "true" to enable CAN-FD frames (default false)
 *
 * Wire format on DataReceived / Send:
 *   [4 bytes: frame ID, big-endian uint32]
 *   [1 byte: flags  -- bit 0 = extended frame (EFF), bit 1 = RTR, bit 2 = FD]
 *   [1 byte: payload length (DLC, 0-8 classic / 0-64 FD)]
 *   [N bytes: payload]
 *
 * Minimum 6 bytes.  Extra bytes in a Send() buffer are silently truncated.
 */
class CanTransport : public Core::ITransport {
    Q_OBJECT

public:
    explicit CanTransport(QObject* parent = nullptr);
    ~CanTransport() override;

    // ── ITransport ──────────────────────────────────────────────────────────
    bool     Connect(const QVariantMap& config) override;
    void     Disconnect() override;
    bool     Send(const QByteArray& data) override;
    State    GetState() const override;
    Type     GetType() const override;
    QString  GetName() const override;
    QVariantMap GetConfig() const override;

    // ── CAN-specific ────────────────────────────────────────────────────────

    /// Serialise a QCanBusFrame to the wire format expected by Send().
    [[nodiscard]] static QByteArray SerialiseFrame(const QCanBusFrame& frame);

    /// Deserialise one frame from a wire-format byte array.
    /// Returns an invalid frame if @p data is malformed.
    [[nodiscard]] static QCanBusFrame DeserialiseFrame(const QByteArray& data);

private slots:
    void OnFramesReceived();
    void OnErrorOccurred(QCanBusDevice::CanBusError error);
    void OnStateChanged(QCanBusDevice::CanBusDeviceState device_state);

private:
    void SetState(State new_state);
    bool ConfigureDevice();

    std::unique_ptr<QCanBusDevice> device_;
    std::atomic<State>             state_{State::Disconnected};
    QVariantMap                    config_;
    bool                           can_fd_{false};
};

} // namespace Transport
} // namespace LinkForge
