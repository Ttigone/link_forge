#include "channel_manager.h"
#include <QDebug>

namespace LinkForge {
namespace Core {

ChannelManager::ChannelManager(QObject* parent)
    : QObject(parent)
{}

ChannelManager::~ChannelManager()
{
    RemoveAllChannels();
}

bool ChannelManager::AddChannel(const QString&     channel_id,
                                 const QString&     transport_type,
                                 const QString&     protocol_type,
                                 const QVariantMap& transport_config,
                                 const QVariantMap& protocol_config)
{
    QMutexLocker lock(&mutex_);

    if (channels_.count(channel_id) > 0) {
        qWarning() << "ChannelManager: channel already exists:" << channel_id;
        return false;
    }

    auto mgr = std::make_unique<CommunicationManager>(nullptr);
    if (!mgr->Initialize(transport_type, protocol_type,
                         transport_config, protocol_config)) {
        qWarning() << "ChannelManager: failed to initialise channel:" << channel_id;
        return false;
    }

    WireChannel(channel_id, mgr.get());
    channels_.emplace(channel_id, Entry{std::move(mgr)});

    lock.unlock();
    emit ChannelAdded(channel_id);
    qInfo() << "ChannelManager: channel added:" << channel_id
            << "(" << transport_type << "/" << protocol_type << ")";
    return true;
}

bool ChannelManager::UpdateChannel(const QString&     channel_id,
                                    const QString&     transport_type,
                                    const QString&     protocol_type,
                                    const QVariantMap& transport_config,
                                    const QVariantMap& protocol_config)
{
    QMutexLocker lock(&mutex_);

    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        qWarning() << "ChannelManager: channel not found for update:" << channel_id;
        return false;
    }

    CommunicationManager* mgr = it->second.manager.get();
    // Disconnect all existing signal/slot wiring before re-init.
    QObject::disconnect(mgr, nullptr, this, nullptr);

    const bool ok = mgr->Initialize(transport_type, protocol_type,
                                    transport_config, protocol_config);
    if (ok) {
        WireChannel(channel_id, mgr);
    }
    return ok;
}

void ChannelManager::RemoveChannel(const QString& channel_id)
{
    QMutexLocker lock(&mutex_);

    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        return;
    }

    it->second.manager->Disconnect();
    QObject::disconnect(it->second.manager.get(), nullptr, this, nullptr);
    channels_.erase(it);

    lock.unlock();
    emit ChannelRemoved(channel_id);
    qInfo() << "ChannelManager: channel removed:" << channel_id;
}

void ChannelManager::RemoveAllChannels()
{
    const QStringList ids = ChannelIds();
    for (const QString& id : ids) {
        RemoveChannel(id);
    }
}

void ChannelManager::Connect(const QString& channel_id)
{
    if (auto* mgr = GetChannel(channel_id)) {
        mgr->Connect();
    }
}

void ChannelManager::Disconnect(const QString& channel_id)
{
    if (auto* mgr = GetChannel(channel_id)) {
        mgr->Disconnect();
    }
}

void ChannelManager::ConnectAll()
{
    const QStringList ids = ChannelIds();
    for (const QString& id : ids) {
        Connect(id);
    }
}

void ChannelManager::DisconnectAll()
{
    const QStringList ids = ChannelIds();
    for (const QString& id : ids) {
        Disconnect(id);
    }
}

bool ChannelManager::SendMessage(const QString& channel_id, const QVariantMap& payload)
{
    if (auto* mgr = GetChannel(channel_id)) {
        return mgr->SendMessage(payload);
    }
    emit ChannelError(channel_id, QStringLiteral("Channel not found"));
    return false;
}

bool ChannelManager::SendRawData(const QString& channel_id, const QByteArray& data)
{
    if (auto* mgr = GetChannel(channel_id)) {
        return mgr->SendRawData(data);
    }
    emit ChannelError(channel_id, QStringLiteral("Channel not found"));
    return false;
}

bool ChannelManager::HasChannel(const QString& channel_id) const
{
    QMutexLocker lock(&mutex_);
    return channels_.count(channel_id) > 0;
}

QStringList ChannelManager::ChannelIds() const
{
    QMutexLocker lock(&mutex_);
    QStringList ids;
    for (const auto& [id, _] : channels_) {
        ids.append(id);
    }
    return ids;
}

int ChannelManager::ChannelCount() const
{
    QMutexLocker lock(&mutex_);
    return static_cast<int>(channels_.size());
}

CommunicationManager* ChannelManager::GetChannel(const QString& channel_id) const
{
    QMutexLocker lock(&mutex_);
    auto it = channels_.find(channel_id);
    return (it != channels_.end()) ? it->second.manager.get() : nullptr;
}

CommunicationManager::State ChannelManager::ChannelState(const QString& channel_id) const
{
    if (const auto* mgr = GetChannel(channel_id)) {
        return mgr->GetState();
    }
    return CommunicationManager::State::Error;
}

void ChannelManager::WireChannel(const QString& channel_id, CommunicationManager* mgr)
{
    QObject::connect(mgr, &CommunicationManager::StateChanged, this,
        [this, channel_id](CommunicationManager::State s) {
            emit ChannelStateChanged(channel_id, s);
        });
    QObject::connect(mgr, &CommunicationManager::MessageReceived, this,
        [this, channel_id](const IProtocol::Message& msg) {
            emit ChannelMessageReceived(channel_id, msg);
        });
    QObject::connect(mgr, &CommunicationManager::RawDataReceived, this,
        [this, channel_id](const QByteArray& data) {
            emit ChannelRawDataReceived(channel_id, data);
        });
    QObject::connect(mgr, &CommunicationManager::DataSent, this,
        [this, channel_id](qint64 bytes) {
            emit ChannelDataSent(channel_id, bytes);
        });
    QObject::connect(mgr, &CommunicationManager::ErrorOccurred, this,
        [this, channel_id](const QString& err) {
            emit ChannelError(channel_id, err);
        });
}

} // namespace Core
} // namespace LinkForge
