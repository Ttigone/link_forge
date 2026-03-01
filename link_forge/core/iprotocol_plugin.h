#pragma once

#include <QtPlugin>
#include <QStringList>
#include <QVariantMap>

#include "iprotocol.h"

namespace LinkForge {
namespace Core {

/**
 * @brief Qt plugin interface -- Protocol providers.
 *
 * Mirrors ITransportPlugin for the protocol layer.  A plugin that provides
 * protocol implementations must implement every pure-virtual method below.
 */
class IProtocolPlugin {
public:
    virtual ~IProtocolPlugin() = default;

    [[nodiscard]] virtual QString PluginName()    const = 0;
    [[nodiscard]] virtual QString PluginVersion() const = 0;

    /// Names of all protocol types this plugin provides (e.g., {"J1939", "CANopen"}).
    [[nodiscard]] virtual QStringList SupportedTypes() const = 0;

    [[nodiscard]] virtual QString TypeDescription(const QString& type_name) const = 0;

    [[nodiscard]] virtual QVariantMap DefaultConfig(const QString& type_name) const = 0;

    [[nodiscard]] virtual IProtocol::Ptr CreateProtocol(const QString&     type_name,
                                                         const QVariantMap& config,
                                                         QObject*           parent) const = 0;
};

} // namespace Core
} // namespace LinkForge

#define IProtocolPlugin_IID "LinkForge.Core.IProtocolPlugin/1.0"
Q_DECLARE_INTERFACE(LinkForge::Core::IProtocolPlugin, IProtocolPlugin_IID)
