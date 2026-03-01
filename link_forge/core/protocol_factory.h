#pragma once

#include <functional>
#include <map>
#include <memory>
#include <QString>
#include <QVariantMap>

#include "iprotocol.h"

namespace LinkForge {
namespace Core {

class ProtocolFactory {
public:
    using CreatorFunc = std::function<IProtocol::Ptr(const QVariantMap&, QObject*)>;

    [[nodiscard]] static IProtocol::Ptr Create(IProtocol::Type type,
                                                const QVariantMap& config = {},
                                                QObject* parent = nullptr);

    [[nodiscard]] static IProtocol::Ptr Create(const QString& type_name,
                                                const QVariantMap& config = {},
                                                QObject* parent = nullptr);

    /// Register or replace a creator by enum type (built-in types).
    static void RegisterProtocol(IProtocol::Type type, CreatorFunc creator);

    /**
     * @brief Register a creator by string name (for plugins and dynamic types).
     * @see TransportFactory::RegisterTransport(const QString&, CreatorFunc)
     */
    static void RegisterProtocol(const QString& type_name, CreatorFunc creator);

    /// Names of all currently registered protocols (enum + string maps).
    [[nodiscard]] static QStringList AvailableTypes();

private:
    ProtocolFactory() = default;

    static std::map<IProtocol::Type, CreatorFunc>& GetCreators();
    static std::map<QString, CreatorFunc>&         GetStringCreators();
    static void            InitDefaultCreators();
    static IProtocol::Type StringToType(const QString& type_name);
    static QString         TypeToString(IProtocol::Type type);
};

} // namespace Core
} // namespace LinkForge
