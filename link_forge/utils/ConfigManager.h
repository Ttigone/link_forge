#pragma once

#include <QObject>
#include <QVariantMap>
#include <QString>
#include <QSettings>
#include <memory>

namespace LinkForge {
namespace Utils {

/**
 * @brief Singleton application configuration manager.
 *
 * Persists and loads JSON configuration files.
 * Provides typed accessors for transport and protocol configs.
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    static ConfigManager& Instance();

    [[nodiscard]] bool LoadConfig(const QString& file_path = {});
    [[nodiscard]] bool SaveConfig(const QString& file_path = {});

    [[nodiscard]] QVariant    GetValue(const QString& key, const QVariant& default_value = {}) const;
    void                       SetValue(const QString& key, const QVariant& value);

    [[nodiscard]] QVariantMap  GetTransportConfig(const QString& transport_type) const;
    void                       SetTransportConfig(const QString& transport_type, const QVariantMap& config);

    [[nodiscard]] QVariantMap  GetProtocolConfig(const QString& protocol_type) const;
    void                       SetProtocolConfig(const QString& protocol_type, const QVariantMap& config);

    void ResetToDefaults();

signals:
    void ConfigChanged(const QString& key, const QVariant& value);

private:
    ConfigManager();
    ~ConfigManager() override = default;

    ConfigManager(const ConfigManager&)            = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void   InitDefaults();
    [[nodiscard]] QString DefaultConfigPath() const;

    std::unique_ptr<QSettings> settings_;
    QVariantMap                config_;
    QString                    current_file_path_;
};

} // namespace Utils
} // namespace LinkForge
