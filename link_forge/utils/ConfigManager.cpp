#include "ConfigManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace LinkForge {
namespace Utils {

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
    : QObject(nullptr)
{
    InitDefaults();
}

void ConfigManager::InitDefaults()
{
    config_["app/name"]        = QStringLiteral("LinkForge");
    config_["app/version"]     = QStringLiteral("1.0.0");
    config_["app/maxMessages"] = 1000;

    // Serial defaults
    QVariantMap serial;
    serial["port"]        = QStringLiteral("COM1");
    serial["baudRate"]    = 9600;
    serial["dataBits"]    = 8;
    serial["parity"]      = QStringLiteral("None");
    serial["stopBits"]    = QStringLiteral("1");
    serial["flowControl"] = QStringLiteral("None");
    config_["transport/serial"] = serial;

    // TCP defaults
    QVariantMap tcp;
    tcp["host"]    = QStringLiteral("127.0.0.1");
    tcp["port"]    = 8080;
    tcp["timeout"] = 3000;
    config_["transport/tcp"] = tcp;

    // UDP defaults
    QVariantMap udp;
    udp["localHost"]  = QStringLiteral("0.0.0.0");
    udp["localPort"]  = 0;
    udp["remoteHost"] = QStringLiteral("127.0.0.1");
    udp["remotePort"] = 8080;
    config_["transport/udp"] = udp;

    // Modbus defaults
    QVariantMap modbus;
    modbus["mode"]    = QStringLiteral("RTU");
    modbus["slaveId"] = 1;
    modbus["timeout"] = 1000;
    config_["protocol/modbus"] = modbus;

    // Custom protocol defaults
    QVariantMap custom;
    custom["subType"] = QStringLiteral("binary");
    config_["protocol/custom"] = custom;
}

bool ConfigManager::LoadConfig(const QString& file_path)
{
    const QString path = file_path.isEmpty() ? DefaultConfigPath() : file_path;

    QFile file(path);
        qWarning() << "ConfigManager: cannot open" << path;
        return false;
    }

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);

    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "ConfigManager: JSON error:" << parse_error.errorString();
        return false;
    }

        return false;
    }

    config_            = doc.object().toVariantMap();
    current_file_path_ = path;
    qInfo() << "ConfigManager: loaded from" << path;
    return true;
}

bool ConfigManager::SaveConfig(const QString& file_path)
{
    const QString path = file_path.isEmpty()
                         ? (current_file_path_.isEmpty() ? DefaultConfigPath() : current_file_path_)
                         : file_path;

    QFileInfo fi(path);
    QDir dir = fi.dir();
        dir.mkpath(".");
    }

    QFile file(path);
        qWarning() << "ConfigManager: cannot write to" << path;
        return false;
    }

    file.write(QJsonDocument::fromVariant(config_).toJson(QJsonDocument::Indented));
    current_file_path_ = path;
    qInfo() << "ConfigManager: saved to" << path;
    return true;
}

QVariant ConfigManager::GetValue(const QString& key, const QVariant& default_value) const
{
    return config_.value(key, default_value);
}

void ConfigManager::SetValue(const QString& key, const QVariant& value)
{
    config_[key] = value;
    emit ConfigChanged(key, value);
}

QVariantMap ConfigManager::GetTransportConfig(const QString& transport_type) const
{
    return config_.value(QString("transport/%1").arg(transport_type.toLower())).toMap();
}

void ConfigManager::SetTransportConfig(const QString& transport_type, const QVariantMap& config)
{
    const QString key = QString("transport/%1").arg(transport_type.toLower());
    config_[key] = config;
    emit ConfigChanged(key, config);
}

QVariantMap ConfigManager::GetProtocolConfig(const QString& protocol_type) const
{
    return config_.value(QString("protocol/%1").arg(protocol_type.toLower())).toMap();
}

void ConfigManager::SetProtocolConfig(const QString& protocol_type, const QVariantMap& config)
{
    const QString key = QString("protocol/%1").arg(protocol_type.toLower());
    config_[key] = config;
    emit ConfigChanged(key, config);
}

void ConfigManager::ResetToDefaults()
{
    config_.clear();
    InitDefaults();
    qInfo() << "ConfigManager: reset to defaults";
}

QString ConfigManager::DefaultConfigPath() const
{
    const QString config_dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(config_dir).filePath("linkforge_config.json");
}

} // namespace Utils
} // namespace LinkForge
