#pragma once

#include <functional>
#include <map>
#include <memory>
#include <QString>

#include "itransport.h"

namespace LinkForge {
namespace Core {

class TransportFactory {
public:
    using CreatorFunc = std::function<ITransport::Ptr(QObject*)>;

    /// Create a transport by enum type.
    [[nodiscard]] static ITransport::Ptr Create(ITransport::Type type,
                                                 QObject* parent = nullptr);

    /// Create a transport by name ("Serial", "TCP", "UDP", etc.).
    [[nodiscard]] static ITransport::Ptr Create(const QString& type_name,
                                                 QObject* parent = nullptr);

    /// Register or replace a creator by enum type (built-in types).
    static void RegisterTransport(ITransport::Type type, CreatorFunc creator);

    /**
     * @brief Register a creator by string name (for plugins and dynamic types).
     *
     * Plugin implementations call this overload since their types are not
     * known at compile time and therefore have no enum value.
     * The name is stored in a separate string-keyed map; Create(QString) will
     * find it even when StringToType() returns Unknown.
     */
    static void RegisterTransport(const QString& type_name, CreatorFunc creator);

    /// Names of all currently registered transports (enum + string maps).
    [[nodiscard]] static QStringList AvailableTypes();

private:
    TransportFactory() = default;

    static std::map<ITransport::Type, CreatorFunc>& GetCreators();
    static std::map<QString, CreatorFunc>&          GetStringCreators();
    static void             InitDefaultCreators();
    static ITransport::Type StringToType(const QString& type_name);
    static QString          TypeToString(ITransport::Type type);
};

} // namespace Core
} // namespace LinkForge
