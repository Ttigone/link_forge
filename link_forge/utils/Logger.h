#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <memory>

namespace LinkForge {
namespace Utils {

/**
 * @brief Thread-safe singleton logger.
 *
 * Writes to stdout and (optionally) a file.
 * Can replace Qt's default message handler via Install().
 */
class Logger : public QObject {
    Q_OBJECT

public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    Q_ENUM(Level)

    static Logger& Instance();

    void SetLevel(Level level) { level_ = level; }
    [[nodiscard]] Level GetLevel() const { return level_; }

    /// Open a log file for appending.  Returns false on failure.
    [[nodiscard]] bool EnableFileLogging(const QString& file_path);
    void               DisableFileLogging();

    void Log(Level level, const QString& message, const QString& category = {});

    // Convenience overloads
    void Debug(const QString& message,    const QString& category = {});
    void Info(const QString& message,     const QString& category = {});
    void Warning(const QString& message,  const QString& category = {});
    void Error(const QString& message,    const QString& category = {});
    void Critical(const QString& message, const QString& category = {});

    /// Install as Qt message handler (routes qDebug / qInfo / etc. here).
    static void Install();

signals:
    void LogMessageGenerated(const QString& message);

private:
    Logger();
    ~Logger() override;

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    static void MessageHandler(QtMsgType type,
                                const QMessageLogContext& context,
                                const QString& msg);

    [[nodiscard]] QString FormatMessage(Level level,
                                         const QString& message,
                                         const QString& category) const;
    [[nodiscard]] static const char* LevelToString(Level level);

    Level                        level_{Level::Info};
    std::unique_ptr<QFile>       log_file_;
    std::unique_ptr<QTextStream> log_stream_;
    QMutex                       mutex_;
};

// ---------------------------------------------------------------------------
// Convenience macros -- use sparingly; prefer direct Logger::Instance() calls.
// ---------------------------------------------------------------------------
#define LOG_DEBUG(msg)    LinkForge::Utils::Logger::Instance().Debug(msg)
#define LOG_INFO(msg)     LinkForge::Utils::Logger::Instance().Info(msg)
#define LOG_WARNING(msg)  LinkForge::Utils::Logger::Instance().Warning(msg)
#define LOG_ERROR(msg)    LinkForge::Utils::Logger::Instance().Error(msg)
#define LOG_CRITICAL(msg) LinkForge::Utils::Logger::Instance().Critical(msg)

} // namespace Utils
} // namespace LinkForge
