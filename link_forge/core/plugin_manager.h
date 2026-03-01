#pragma once

#include <QObject>
#include <QMutex>
#include <QStringList>
#include <memory>
#include <vector>

class QPluginLoader;

namespace LinkForge {
namespace Core {

/**
 * @brief Dynamically loads transport / protocol plugin DLLs.
 *
 * Workflow:
 *   1. Call LoadPluginsFromDir() once at startup (or when the user points to
 *      a plugin directory).
 *   2. Each loadable file is probed for ITransportPlugin and IProtocolPlugin.
 *   3. Discovered implementations are registered directly in TransportFactory
 *      / ProtocolFactory under their SupportedTypes() names.
 *   4. From that point, TransportFactory::Create("VectorCAN") works transparently.
 *
 * Thread safety: all methods are guarded by an internal mutex and safe to call
 * from any thread.
 */
class PluginManager : public QObject {
    Q_OBJECT

public:
    static PluginManager& Instance();

    /**
     * @brief Scan a directory and load every valid plugin file found.
     * @return Number of plugin files successfully loaded.
     */
    int  LoadPluginsFromDir(const QString& dir_path);

    /**
     * @brief Load a single plugin file.
     * @return true if the file was loaded and at least one interface discovered.
     */
    bool LoadPlugin(const QString& file_path);

    /// Unload all plugins and de-register their types from the factories.
    void UnloadAll();

    [[nodiscard]] QStringList LoadedPluginFiles()       const;
    [[nodiscard]] QStringList RegisteredTransportTypes() const;
    [[nodiscard]] QStringList RegisteredProtocolTypes()  const;

signals:
    void TransportPluginLoaded(const QString& transport_type);
    void ProtocolPluginLoaded(const QString& protocol_type);
    void PluginLoadError(const QString& file_path, const QString& error);

private:
    PluginManager();
    ~PluginManager() override;

    PluginManager(const PluginManager&)            = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    struct Entry {
        std::unique_ptr<QPluginLoader> loader;
        QStringList registered_transport_types;
        QStringList registered_protocol_types;
    };

    std::vector<Entry> entries_;
    mutable QMutex     mutex_;

    QStringList registered_transport_types_;
    QStringList registered_protocol_types_;
};

} // namespace Core
} // namespace LinkForge
