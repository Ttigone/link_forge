#pragma once

#include <QObject>
#include <QMutex>
#include <map>
#include <memory>

#include "communication_manager.h"

namespace LinkForge {
namespace Core {

/**
 * @brief Manages multiple independent communication channels.
 *
 * Each channel is identified by a user-supplied string ID and owns its own
 * CommunicationManager (and therefore its own dedicated I/O thread).
 * Channels are completely independent -- Serial/Modbus, TCP/JSON, CAN/custom
 * can all run simultaneously with no interference.
 *
 * All signals from individual channels are re-emitted here with the
 * channel_id prepended, so a single connection to ChannelManager suffices
 * when an application needs to monitor all channels uniformly.
 *
 * Thread safety: AddChannel / RemoveChannel / all query methods are guarded
 * by an internal mutex and safe to call from any thread.  Connect/Disconnect/
 * Send methods must be called from the owning (usually GUI) thread.
 */
class ChannelManager : public QObject {
    Q_OBJECT

public:
    explicit ChannelManager(QObject* parent = nullptr);
    ~ChannelManager() override;

    /**
     * @brief Create a new channel and configure its transport + protocol.
     *
     * If @p channel_id already exists the call is a no-op and returns false.
     * Use UpdateChannel() to reconfigure an existing channel.
     *
     * @return false if the channel already exists or factory lookup fails.
     */
    [[nodiscard]] bool AddChannel(const QString&     channel_id,
                                   const QString&     transport_type,
                                   const QString&     protocol_type,
                                   const QVariantMap& transport_config,
                                   const QVariantMap& protocol_config = {});

    /**
     * @brief Re-initialise an existing channel with a new configuration.
     *
     * The channel is disconnected and fully re-created.
     * @return false if @p channel_id does not exist or factory lookup fails.
     */
    [[nodiscard]] bool UpdateChannel(const QString&     channel_id,
                                      const QString&     transport_type,
                                      const QString&     protocol_type,
                                      const QVariantMap& transport_config,
                                      const QVariantMap& protocol_config = {});

    /// Remove and tear down one channel (disconnects first).
    void RemoveChannel(const QString& channel_id);

    /// Remove and tear down all channels.
    void RemoveAllChannels();

    // ── Per-channel control ────────────────────────────────────────────────

    void Connect(const QString& channel_id);
    void Disconnect(const QString& channel_id);
    void ConnectAll();
    void DisconnectAll();

    [[nodiscard]] bool SendMessage(const QString&     channel_id,
                                    const QVariantMap& payload);
    [[nodiscard]] bool SendRawData(const QString& channel_id,
                                    const QByteArray& data);

    // ── Query ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool HasChannel(const QString& channel_id) const;
    [[nodiscard]] QStringList ChannelIds() const;
    [[nodiscard]] int ChannelCount() const;

    /// Direct access to a channel\'s manager (nullptr if not found).
    [[nodiscard]] CommunicationManager* GetChannel(const QString& channel_id) const;

    [[nodiscard]] CommunicationManager::State ChannelState(const QString& channel_id) const;

signals:
    void ChannelAdded(const QString& channel_id);
    void ChannelRemoved(const QString& channel_id);
    void ChannelStateChanged(const QString& channel_id,
                              LinkForge::Core::CommunicationManager::State state);
    void ChannelMessageReceived(const QString& channel_id,
                                 const LinkForge::Core::IProtocol::Message& message);
    void ChannelRawDataReceived(const QString& channel_id, const QByteArray& data);
    void ChannelDataSent(const QString& channel_id, qint64 bytes);
    void ChannelError(const QString& channel_id, const QString& error);

private:
    void WireChannel(const QString& channel_id, CommunicationManager* mgr);

    struct Entry {
        std::unique_ptr<CommunicationManager> manager;
    };

    std::map<QString, Entry> channels_;
    mutable QMutex           mutex_;
};

} // namespace Core
} // namespace LinkForge
