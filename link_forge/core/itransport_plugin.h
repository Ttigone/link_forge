#pragma once

#include <QtPlugin>
#include <QStringList>
#include <QVariantMap>

#include "itransport.h"

namespace LinkForge {
namespace Core {

/**
 * @brief Qt plugin interface -- Transport providers.
 *
 * A plugin DLL (or statically linked module) that provides one or more
 * transport types must:
 *   1. Inherit from both QObject and ITransportPlugin.
 *   2. Declare Q_INTERFACES(LinkForge::Core::ITransportPlugin).
 *   3. Declare Q_PLUGIN_METADATA(IID ITransportPlugin_IID FILE "plugin.json").
 *   4. Implement every pure-virtual method below.
 *
 * The PluginManager will discover the plugin, call SupportedTypes(), then
 * register a creator in TransportFactory for each supported type name.
 *
 * Example plugin.json:
 * @code
 * { "IID": "LinkForge.Core.ITransportPlugin/1.0", "name": "MyCANPlugin" }
 * @endcode
 */
class ITransportPlugin {
public:
    virtual ~ITransportPlugin() = default;

    /// Human-readable plugin identifier (e.g., "VectorCAN").
    [[nodiscard]] virtual QString PluginName()    const = 0;
    [[nodiscard]] virtual QString PluginVersion() const = 0;

    /**
     * @brief Names of all transport types this plugin provides.
     *
     * These strings are registered with TransportFactory, so every name must
     * be unique across all loaded plugins (and the built-in types).
     * Example: {"VectorCAN", "PCANUSB"}.
     */
    [[nodiscard]] virtual QStringList SupportedTypes() const = 0;

    /**
     * @brief Describe one transport type for display in a UI.
     * @param type_name One of the values returned by SupportedTypes().
     */
    [[nodiscard]] virtual QString TypeDescription(const QString& type_name) const = 0;

    /**
     * @brief Return the default configuration keys / values for a type.
     * @param type_name One of the values returned by SupportedTypes().
     */
    [[nodiscard]] virtual QVariantMap DefaultConfig(const QString& type_name) const = 0;

    /**
     * @brief Instantiate a transport object.
     * @param type_name One of the values returned by SupportedTypes().
     * @param parent    Qt parent (may be nullptr).
     */
    [[nodiscard]] virtual ITransport::Ptr CreateTransport(const QString& type_name,
                                                           QObject* parent) const = 0;
};

} // namespace Core
} // namespace LinkForge

#define ITransportPlugin_IID "LinkForge.Core.ITransportPlugin/1.0"
Q_DECLARE_INTERFACE(LinkForge::Core::ITransportPlugin, ITransportPlugin_IID)
