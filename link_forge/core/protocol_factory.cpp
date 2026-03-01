#include "protocol_factory.h"
#include "type_names.h"

#include <QDebug>
#include <unordered_map>

#include "../protocol/modbus_protocol.h"
#include "../protocol/custom_protocol.h"
#include "../protocol/frame_protocol.h"

namespace LinkForge {
namespace Core {

namespace {

const std::unordered_map<QString, IProtocol::Type>& StringToProtocolMap()
{
    static const std::unordered_map<QString, IProtocol::Type> kMap = {
        {QString(TypeNames::Protocol::kModbus).toLower(),  IProtocol::Type::Modbus},
        {QString(TypeNames::Protocol::kMavLink).toLower(), IProtocol::Type::MAVLink},
        {QString(TypeNames::Protocol::kUds).toLower(),     IProtocol::Type::UDS},
        {QString(TypeNames::Protocol::kDbc).toLower(),     IProtocol::Type::DBC},
        {QString(TypeNames::Protocol::kCustom).toLower(),  IProtocol::Type::Custom},
        {QString(TypeNames::Protocol::kJson).toLower(),    IProtocol::Type::Custom},
    };
    return kMap;
}

const std::unordered_map<IProtocol::Type, QString>& ProtocolToStringMap()
{
    static const std::unordered_map<IProtocol::Type, QString> kMap = {
        {IProtocol::Type::Modbus,  QString(TypeNames::Protocol::kModbus)},
        {IProtocol::Type::MAVLink, QString(TypeNames::Protocol::kMavLink)},
        {IProtocol::Type::UDS,     QString(TypeNames::Protocol::kUds)},
        {IProtocol::Type::DBC,     QString(TypeNames::Protocol::kDbc)},
        {IProtocol::Type::Custom,  QString(TypeNames::Protocol::kCustom)},
        {IProtocol::Type::Unknown, QString(TypeNames::Protocol::kUnknown)},
    };
    return kMap;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

std::map<IProtocol::Type, ProtocolFactory::CreatorFunc>&
ProtocolFactory::GetCreators()
{
    static std::map<IProtocol::Type, CreatorFunc> creators;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        InitDefaultCreators();
    }
    return creators;
}

std::map<QString, ProtocolFactory::CreatorFunc>&
ProtocolFactory::GetStringCreators()
{
    static std::map<QString, CreatorFunc> creators;
    return creators;
}

void ProtocolFactory::InitDefaultCreators()
{
    auto& c = GetCreators();

    c[IProtocol::Type::Modbus] = [](const QVariantMap& config, QObject* parent) -> IProtocol::Ptr {
        const QString mode_str = config.value(QString(TypeNames::Config::kMode),
                                              QString(TypeNames::Modbus::kModeRtu)).toString().toUpper();
        const auto mode = (mode_str == TypeNames::Modbus::kModeTcp)
                          ? Protocol::ModbusProtocol::Mode::TCP
                          : Protocol::ModbusProtocol::Mode::RTU;
        return std::make_shared<Protocol::ModbusProtocol>(mode, parent);
    };

    c[IProtocol::Type::Custom] = [](const QVariantMap& config, QObject* parent) -> IProtocol::Ptr {
        const QString sub = config.value(QString(TypeNames::Config::kSubType),
                                         QString(TypeNames::Protocol::kBinary)).toString().toLower();
        if (sub == TypeNames::Protocol::kJson) {
            return std::make_shared<Protocol::JsonProtocol>(parent);
        }
        return std::make_shared<Protocol::CustomProtocol>(parent);
    };

    // Register Frame protocol in the string-keyed map so it can be
    // looked up by name ("Frame") without needing a dedicated enum value.
    GetStringCreators()[QStringLiteral("Frame")] =
        [](const QVariantMap& config, QObject* parent) -> IProtocol::Ptr {
            return Protocol::FrameProtocol::FromConfig(config, parent);
        };

    qInfo() << "ProtocolFactory: default creators registered (Modbus, Custom, Frame)";
}

IProtocol::Ptr ProtocolFactory::Create(IProtocol::Type type,
                                        const QVariantMap& config,
                                        QObject* parent)
{
    auto& c = GetCreators();
    auto  it = c.find(type);
    if (it != c.end()) {
        return it->second(config, parent);
    }
    qWarning() << "ProtocolFactory: no creator for type" << static_cast<int>(type);
    return nullptr;
}

IProtocol::Ptr ProtocolFactory::Create(const QString& type_name,
                                        const QVariantMap& config,
                                        QObject* parent)
{
    // 1. Try the well-known enum lookup first.
    const IProtocol::Type type = StringToType(type_name);
    if (type != IProtocol::Type::Unknown) {
        return Create(type, config, parent);
    }

    // 2. Fall back to the string-keyed map (plugin / dynamic types including "Frame").
    auto& sc = GetStringCreators();
    auto  it = sc.find(type_name);
    if (it != sc.end()) {
        return it->second(config, parent);
    }

    qWarning() << "ProtocolFactory: unknown type name:" << type_name;
    return nullptr;
}

void ProtocolFactory::RegisterProtocol(IProtocol::Type type, CreatorFunc creator)
{
    GetCreators()[type] = std::move(creator);
    qInfo() << "ProtocolFactory: registered enum type" << static_cast<int>(type);
}

void ProtocolFactory::RegisterProtocol(const QString& type_name, CreatorFunc creator)
{
    GetStringCreators()[type_name] = std::move(creator);
    qInfo() << "ProtocolFactory: registered string type" << type_name;
}

QStringList ProtocolFactory::AvailableTypes()
{
    QStringList names;
    for (const auto& [type, fn] : GetCreators()) {
        names.append(TypeToString(type));
    }
    for (const auto& [name, fn] : GetStringCreators()) {
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

IProtocol::Type ProtocolFactory::StringToType(const QString& type_name)
{
    const auto& map = StringToProtocolMap();
    auto it = map.find(type_name.toLower());
    return (it != map.end()) ? it->second : IProtocol::Type::Unknown;
}

QString ProtocolFactory::TypeToString(IProtocol::Type type)
{
    const auto& map = ProtocolToStringMap();
    auto it = map.find(type);
    return (it != map.end()) ? it->second : QString(TypeNames::Protocol::kUnknown);
}

} // namespace Core
} // namespace LinkForge
