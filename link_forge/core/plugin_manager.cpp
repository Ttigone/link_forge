#include "plugin_manager.h"
#include "itransport_plugin.h"
#include "iprotocol_plugin.h"
#include "transport_factory.h"
#include "protocol_factory.h"

#include <QPluginLoader>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace LinkForge {
namespace Core {

PluginManager& PluginManager::Instance()
{
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager()
    : QObject(nullptr)
{}

PluginManager::~PluginManager()
{
    UnloadAll();
}

int PluginManager::LoadPluginsFromDir(const QString& dir_path)
{
    QDir dir(dir_path);
        qWarning() << "PluginManager: directory does not exist:" << dir_path;
        return 0;
    }

    // Match platform-specific library extensions.
#if defined(Q_OS_WIN)
    const QStringList filters{"*.dll"};
#elif defined(Q_OS_MAC)
    const QStringList filters{"*.dylib", "*.bundle"};
#else
    const QStringList filters{"*.so"};
#endif

    int loaded = 0;
    const auto files = dir.entryList(filters, QDir::Files);
    for (const QString& fn : files) {
        if (LoadPlugin(dir.absoluteFilePath(fn))) {
            ++loaded;
        }
    }

    qInfo() << "PluginManager: loaded" << loaded << "plugin(s) from" << dir_path;
    return loaded;
}

bool PluginManager::LoadPlugin(const QString& file_path)
{
    QMutexLocker lock(&mutex_);

    // Check for duplicate.
    for (const auto& e : entries_) {
        if (e.loader->fileName() == QFileInfo(file_path).absoluteFilePath()) {
            qWarning() << "PluginManager: already loaded:" << file_path;
            return true;
        }
    }

    Entry entry;
    entry.loader = std::make_unique<QPluginLoader>(file_path);

    QObject* instance = entry.loader->instance();
    if (!instance) {
        const QString err = entry.loader->errorString();
        emit PluginLoadError(file_path, err);
        qWarning() << "PluginManager: failed to load" << file_path << "-" << err;
        return false;
    }

    bool found_any = false;

    // Try transport plugin interface.
    if (auto* tp = qobject_cast<ITransportPlugin*>(instance)) {
        for (const QString& type_name : tp->SupportedTypes()) {
            // Capture by value to avoid dangling reference after unload.
            TransportFactory::RegisterTransport(type_name,
                [tp, type_name](QObject* parent) -> ITransport::Ptr {
                    return tp->CreateTransport(type_name, parent);
                });
            entry.registered_transport_types.append(type_name);
            registered_transport_types_.append(type_name);
            emit TransportPluginLoaded(type_name);
            qInfo() << "PluginManager: registered transport type" << type_name
                    << "from" << tp->PluginName() << tp->PluginVersion();
        }
        found_any = true;
    }

    // Try protocol plugin interface.
    if (auto* pp = qobject_cast<IProtocolPlugin*>(instance)) {
        for (const QString& type_name : pp->SupportedTypes()) {
            ProtocolFactory::RegisterProtocol(type_name,
                [pp, type_name](const QVariantMap& config, QObject* parent) -> IProtocol::Ptr {
                    return pp->CreateProtocol(type_name, config, parent);
                });
            entry.registered_protocol_types.append(type_name);
            registered_protocol_types_.append(type_name);
            emit ProtocolPluginLoaded(type_name);
            qInfo() << "PluginManager: registered protocol type" << type_name
                    << "from" << pp->PluginName() << pp->PluginVersion();
        }
        found_any = true;
    }

    if (!found_any) {
        const QString err = QStringLiteral("No recognised plugin interface in ") + file_path;
        emit PluginLoadError(file_path, err);
        qWarning() << "PluginManager:" << err;
        entry.loader->unload();
        return false;
    }

    entries_.push_back(std::move(entry));
    return true;
}

void PluginManager::UnloadAll()
{
    QMutexLocker lock(&mutex_);

    for (auto& e : entries_) {
        // TODO: de-register types from factories when factory supports unregister.
        if (e.loader) {
            e.loader->unload();
        }
    }
    entries_.clear();
    registered_transport_types_.clear();
    registered_protocol_types_.clear();
}

QStringList PluginManager::LoadedPluginFiles() const
{
    QMutexLocker lock(&mutex_);
    QStringList files;
    for (const auto& e : entries_) {
        files.append(e.loader->fileName());
    }
    return files;
}

QStringList PluginManager::RegisteredTransportTypes() const
{
    QMutexLocker lock(&mutex_);
    return registered_transport_types_;
}

QStringList PluginManager::RegisteredProtocolTypes() const
{
    QMutexLocker lock(&mutex_);
    return registered_protocol_types_;
}

} // namespace Core
} // namespace LinkForge
