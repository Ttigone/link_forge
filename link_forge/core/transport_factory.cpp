#include "transport_factory.h"
#include "type_names.h"

#include <QDebug>
#include <unordered_map>

#include "../transport/serial_transport.h"
#include "../transport/socket_transport.h"
#include "../transport/can_transport.h"

namespace LinkForge {
namespace Core {

namespace {

const std::unordered_map<QString, ITransport::Type>& StringToTypeMap()
{
    static const std::unordered_map<QString, ITransport::Type> kMap = {
        {QString(TypeNames::Transport::kSerial).toLower(),    ITransport::Type::Serial},
        {QString(TypeNames::Transport::kTcp).toLower(),       ITransport::Type::TcpSocket},
        {QString(TypeNames::Transport::kUdp).toLower(),       ITransport::Type::UdpSocket},
        {QString(TypeNames::Transport::kCan).toLower(),       ITransport::Type::Can},
        {QString(TypeNames::Transport::kUart).toLower(),      ITransport::Type::Serial},
        {QString(TypeNames::Transport::kTcpSocket).toLower(), ITransport::Type::TcpSocket},
        {QString(TypeNames::Transport::kUdpSocket).toLower(), ITransport::Type::UdpSocket},
    };
    return kMap;
}

const std::unordered_map<ITransport::Type, QString>& TypeToStringMap()
{
    static const std::unordered_map<ITransport::Type, QString> kMap = {
        {ITransport::Type::Serial,    QString(TypeNames::Transport::kSerial)},
        {ITransport::Type::TcpSocket, QString(TypeNames::Transport::kTcp)},
        {ITransport::Type::UdpSocket, QString(TypeNames::Transport::kUdp)},
        {ITransport::Type::Can,       QString(TypeNames::Transport::kCan)},
        {ITransport::Type::Unknown,   QString(TypeNames::Transport::kUnknown)},
    };
    return kMap;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

std::map<ITransport::Type, TransportFactory::CreatorFunc>&
TransportFactory::GetCreators()
{
    static std::map<ITransport::Type, CreatorFunc> creators;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        InitDefaultCreators();
    }
    return creators;
}

std::map<QString, TransportFactory::CreatorFunc>&
TransportFactory::GetStringCreators()
{
    static std::map<QString, CreatorFunc> creators;
    return creators;
}

void TransportFactory::InitDefaultCreators()
{
    auto& c = GetCreators();

    c[ITransport::Type::Serial] = [](QObject* parent) -> ITransport::Ptr {
        return std::make_shared<Transport::SerialTransport>(parent);
    };
    c[ITransport::Type::TcpSocket] = [](QObject* parent) -> ITransport::Ptr {
        return std::make_shared<Transport::TcpSocketTransport>(parent);
    };
    c[ITransport::Type::UdpSocket] = [](QObject* parent) -> ITransport::Ptr {
        return std::make_shared<Transport::UdpSocketTransport>(parent);
    };
    c[ITransport::Type::Can] = [](QObject* parent) -> ITransport::Ptr {
        return std::make_shared<Transport::CanTransport>(parent);
    };

    qInfo() << "TransportFactory: default creators registered (Serial, TCP, UDP, CAN)";
}

ITransport::Ptr TransportFactory::Create(ITransport::Type type, QObject* parent)
{
    auto& c = GetCreators();
    auto  it = c.find(type);
    if (it != c.end()) {
        return it->second(parent);
    }
    qWarning() << "TransportFactory: no creator for type" << static_cast<int>(type);
    return nullptr;
}

ITransport::Ptr TransportFactory::Create(const QString& type_name, QObject* parent)
{
    // 1. Try the well-known enum lookup first.
    const ITransport::Type type = StringToType(type_name);
    if (type != ITransport::Type::Unknown) {
        return Create(type, parent);
    }

    // 2. Fall back to the string-keyed map (plugin / dynamic types).
    auto& sc = GetStringCreators();
    auto  it = sc.find(type_name);
    if (it != sc.end()) {
        return it->second(parent);
    }

    qWarning() << "TransportFactory: unknown type name:" << type_name;
    return nullptr;
}

void TransportFactory::RegisterTransport(ITransport::Type type, CreatorFunc creator)
{
    GetCreators()[type] = std::move(creator);
    qInfo() << "TransportFactory: registered enum type" << static_cast<int>(type);
}

void TransportFactory::RegisterTransport(const QString& type_name, CreatorFunc creator)
{
    GetStringCreators()[type_name] = std::move(creator);
    qInfo() << "TransportFactory: registered string type" << type_name;
}

QStringList TransportFactory::AvailableTypes()
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

ITransport::Type TransportFactory::StringToType(const QString& type_name)
{
    const auto& map = StringToTypeMap();
    auto it = map.find(type_name.toLower());
    return (it != map.end()) ? it->second : ITransport::Type::Unknown;
}

QString TransportFactory::TypeToString(ITransport::Type type)
{
    const auto& map = TypeToStringMap();
    auto it = map.find(type);
    return (it != map.end()) ? it->second : QString(TypeNames::Transport::kUnknown);
}

} // namespace Core
} // namespace LinkForge
