#pragma once

#include "../core/iprotocol.h"
#include <QMutex>
#include <unordered_map>

namespace LinkForge {
namespace Protocol {

/**
 * @brief Modbus RTU / TCP protocol.
 *
 * State machine: the receive-buffer accumulation and frame dispatch are
 * driven entirely by an enum-based switch -- no string comparisons at runtime.
 */
class ModbusProtocol : public Core::IProtocol {
    Q_OBJECT

public:
    enum class Mode {
        RTU,  ///< Serial framing with CRC-16.
        TCP   ///< Ethernet framing with MBAP header.
    };
    Q_ENUM(Mode)

    enum class FunctionCode : quint8 {
        ReadCoils              = 0x01,
        ReadDiscreteInputs     = 0x02,
        ReadHoldingRegisters   = 0x03,
        ReadInputRegisters     = 0x04,
        WriteSingleCoil        = 0x05,
        WriteSingleRegister    = 0x06,
        WriteMultipleCoils     = 0x0F,
        WriteMultipleRegisters = 0x10,
        Unknown                = 0xFF,
    };
    Q_ENUM(FunctionCode)

    explicit ModbusProtocol(Mode mode, QObject* parent = nullptr);
    ~ModbusProtocol() override = default;

    [[nodiscard]] std::optional<QByteArray> BuildMessage(const QVariantMap& payload) override;
    [[nodiscard]] std::vector<Message>       ParseData(const QByteArray& data)         override;
    [[nodiscard]] bool                       ValidateMessage(const QByteArray& data) const override;

    [[nodiscard]] Type    GetType() const override { return Type::Modbus; }
    [[nodiscard]] QString GetName() const override { return QStringLiteral("Modbus"); }
    void Reset() override;

    [[nodiscard]] Mode GetMode() const { return mode_; }

private:
    [[nodiscard]] static quint16     CalculateCRC(const QByteArray& data);
    [[nodiscard]] static FunctionCode FunctionNameToCode(const QString& name);

    [[nodiscard]] QByteArray BuildReadRequest(quint8 slave_id, FunctionCode func,
                                              quint16 address, quint16 quantity);
    [[nodiscard]] QByteArray BuildWriteRequest(quint8 slave_id, FunctionCode func,
                                               quint16 address, const QVariantList& values);
    [[nodiscard]] Message    ParseResponse(const QByteArray& data);

    Mode       mode_;
    QByteArray receive_buffer_;
    QMutex     mutex_;
};

} // namespace Protocol
} // namespace LinkForge
