#include "Logger.h"
#include <QDir>

namespace LinkForge {
namespace Utils {

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : QObject(nullptr)
{}

Logger::~Logger()
{
    DisableFileLogging();
}

bool Logger::EnableFileLogging(const QString& file_path)
{
    QMutexLocker locker(&mutex_);

    if (log_file_ && log_file_->isOpen()) {
        log_stream_.reset();
        log_file_->close();
    }

    QFileInfo fi(file_path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    log_file_ = std::make_unique<QFile>(file_path);
    if (!log_file_->open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Logger: failed to open" << file_path;
        log_file_.reset();
        return false;
    }

    log_stream_ = std::make_unique<QTextStream>(log_file_.get());
    Log(Level::Info, QString("Log file opened: %1").arg(file_path), "Logger");
    return true;
}

void Logger::DisableFileLogging()
{
    QMutexLocker locker(&mutex_);
    if (log_stream_) {
        log_stream_->flush();
        log_stream_.reset();
    }
    if (log_file_ && log_file_->isOpen()) {
        log_file_->close();
        log_file_.reset();
    }
}

void Logger::Log(Level level, const QString& message, const QString& category)
{
    if (level < level_) {
        return;
    }
    QMutexLocker locker(&mutex_);

    const QString formatted = FormatMessage(level, message, category);

    QTextStream console(stdout);
    console << formatted << Qt::endl;

    if (log_stream_) {
        *log_stream_ << formatted << Qt::endl;
        log_stream_->flush();
    }

    emit LogMessageGenerated(formatted);
}

void Logger::Debug(const QString& message, const QString& category)    { Log(Level::Debug,    message, category); }
void Logger::Info(const QString& message, const QString& category)     { Log(Level::Info,     message, category); }
void Logger::Warning(const QString& message, const QString& category)  { Log(Level::Warning,  message, category); }
void Logger::Error(const QString& message, const QString& category)    { Log(Level::Error,    message, category); }
void Logger::Critical(const QString& message, const QString& category) { Log(Level::Critical, message, category); }

void Logger::Install()
{
    qInstallMessageHandler(Logger::MessageHandler);
}

void Logger::MessageHandler(QtMsgType type,
                             const QMessageLogContext& context,
                             const QString& msg)
{
    Level level = Level::Info;
    switch (type) {
        case QtDebugMsg:    level = Level::Debug;    break;
        case QtInfoMsg:     level = Level::Info;     break;
        case QtWarningMsg:  level = Level::Warning;  break;
        case QtCriticalMsg: level = Level::Error;    break;
        case QtFatalMsg:    level = Level::Critical; break;
    }

    const QString category = context.category ? QString(context.category) : QString();
    Logger::Instance().Log(level, msg, category);

    if (type == QtFatalMsg) {
        abort();
    }
}

QString Logger::FormatMessage(Level level,
                               const QString& message,
                               const QString& category) const
{
    const QString ts  = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    const char*   lvl = LevelToString(level);

    if (category.isEmpty()) {
        return QString("[%1] [%2] %3").arg(ts, QLatin1String(lvl), message);
    }
    return QString("[%1] [%2] [%3] %4").arg(ts, QLatin1String(lvl), category, message);
}

const char* Logger::LevelToString(Level level)
{
    switch (level) {
        case Level::Debug:    return "DEBUG";
        case Level::Info:     return "INFO ";
        case Level::Warning:  return "WARN ";
        case Level::Error:    return "ERROR";
        case Level::Critical: return "CRIT ";
    }
    return "?????";
}

} // namespace Utils
} // namespace LinkForge
