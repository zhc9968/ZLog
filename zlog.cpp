#include "zlog.h"
#include "zlogviewer.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <csignal>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <comdef.h>
#endif

// 静态成员初始化
ZLog* ZLog::s_instance = nullptr;
QtMessageHandler ZLog::s_oldHandler = nullptr;
bool ZLog::s_globalPopupEnabled = true;
QWidget* ZLog::s_defaultPopupParent = nullptr;
bool ZLog::s_logViewerEnabled = true;
ZLogViewer* ZLog::s_logViewer = nullptr;
#ifdef Q_OS_WIN
HWND ZLog::s_defaultPopupHwnd = nullptr;
#endif

ZLog::ZLog(QObject *parent)
    : QObject(parent), m_minLevel(DEBUG), m_rolling(SizeRolling),
    m_maxSize(10*1024*1024), m_maxBackups(5), m_consoleOutput(true),
    m_format("{time} [{level}] {thread} {file}:{line} - {function}: {message}")
{
}

ZLog::~ZLog()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
#ifdef Q_OS_WIN
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);
#endif
}

ZLog* ZLog::instance()
{
    if (!s_instance)
        s_instance = new ZLog();
    return s_instance;
}

void ZLog::init(const QString &appName,
                const QString &logFilePath, Level minLevel,
                FileRolling rolling, qint64 maxSizeBytes, int maxBackups,
                bool consoleOutput, const QString &logFormat)
{
    QMutexLocker lock(&m_mutex);
    m_appName = appName;
    m_filePath = logFilePath;
    m_minLevel = minLevel;
    m_rolling = rolling;
    m_maxSize = maxSizeBytes;
    m_maxBackups = maxBackups;
    m_consoleOutput = consoleOutput;
    m_format = logFormat;

    QFileInfo fi(logFilePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    openLogFile();
}

void ZLog::setGlobalPopupEnabled(bool enabled) { s_globalPopupEnabled = enabled; }
bool ZLog::globalPopupEnabled() { return s_globalPopupEnabled; }

void ZLog::setDefaultPopupParent(QWidget *parent) { s_defaultPopupParent = parent; }
#ifdef Q_OS_WIN
void ZLog::setDefaultPopupParent(HWND hwnd) { s_defaultPopupHwnd = hwnd; }
#endif
QWidget* ZLog::defaultPopupParent() { return s_defaultPopupParent; }

void ZLog::setLogViewerEnabled(bool enabled) { s_logViewerEnabled = enabled; }
bool ZLog::isLogViewerEnabled() { return s_logViewerEnabled; }
void ZLog::showLogViewer()
{
    if (!s_logViewerEnabled) return;
    if (!s_logViewer) {
        s_logViewer = new ZLogViewer();
        s_logViewer->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    s_logViewer->show();
    s_logViewer->raise();
}
void ZLog::hideLogViewer() { if (s_logViewer) s_logViewer->hide(); }
ZLogViewer* ZLog::logViewer() { return s_logViewer; }

void ZLog::setMinLevel(Level level) { QMutexLocker lock(&m_mutex); m_minLevel = level; }
void ZLog::setConsoleOutput(bool enabled) { QMutexLocker lock(&m_mutex); m_consoleOutput = enabled; }
void ZLog::setFormat(const QString &format) { QMutexLocker lock(&m_mutex); m_format = format; }

void ZLog::write(Level level, const QString &file, int line,
                 const QString &function, const QString &message,
                 bool forcePopup, bool forceNoPopup, bool blocking,
                 QWidget *parent
#ifdef Q_OS_WIN
                 , HWND hwndParent
#endif
                 )
{
    if (level < m_minLevel) return;
    quint64 id = generateId();
    doWrite(id, level, file, line, function, message);

    bool popup = false;
    if (forceNoPopup) popup = false;
    else if (forcePopup) popup = true;
    else popup = (level != DEBUG) && s_globalPopupEnabled;

    if (popup) {
        QWidget *p = parent ? parent : s_defaultPopupParent;
        showPopup(level, message, blocking, p);
    }

    emit newMessage(id, level, message, file, line, function,
                    (quintptr)QThread::currentThread());
}

void ZLog::doWrite(quint64 id, Level level, const QString &file, int line,
                   const QString &function, const QString &message)
{
    QMutexLocker lock(&m_mutex);
    // 固定使用 JSON 格式（强制 ID + 结构化）
    QJsonObject obj;
    obj["id"] = QString::number(id);
    obj["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    obj["level"] = levelToString(level);
    obj["thread"] = QString::number((quintptr)QThread::currentThread());
    obj["file"] = QFileInfo(file).fileName();
    obj["line"] = line;
    obj["function"] = function;
    obj["message"] = message;
    QString formatted = QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    if (!m_file.isOpen()) {
        if (!openLogFile()) return;
    }

    m_stream << formatted << Qt::endl;
    m_stream.flush();

    if (m_rolling == SizeRolling && m_file.size() >= m_maxSize) {
        rollFiles();
    }

    if (m_consoleOutput) {
        QString text = formatLog(id, level, file, line, function, message);
        switch (level) {
        case DEBUG:    qDebug().noquote() << text; break;
        case INFO:     qDebug().noquote() << text; break;
        case QUESTION: qDebug().noquote() << text; break;
        case WARNING:  qWarning().noquote() << text; break;
        case CRITICAL: qCritical().noquote() << text; break;
        }
    }
}

void ZLog::showPopup(Level level, const QString &message, bool blocking, QWidget *parent)
{
    QMessageBox::Icon icon;
    QString title = m_appName + " - ";

    switch (level) {
    case INFO:     icon = QMessageBox::Information; title += "提示"; break;
    case QUESTION: icon = QMessageBox::Question;    title += "询问"; break;
    case WARNING:  icon = QMessageBox::Warning;     title += "警告"; break;
    case CRITICAL: icon = QMessageBox::Critical;    title += "严重错误"; break;
    default:       return;
    }

    QString fullMsg = QString("[%1]\n%2").arg(levelToString(level), message);

    if (blocking) {
        auto showFunc = [icon, title, fullMsg, parent]() {
            QMessageBox msgBox(icon, title, fullMsg, QMessageBox::Ok, parent);
            msgBox.exec();
        };
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            showFunc();
        } else {
            QMetaObject::invokeMethod(QApplication::instance(), showFunc, Qt::BlockingQueuedConnection);
        }
    } else {
        auto showFunc = [icon, title, fullMsg, parent]() {
            QMessageBox *msgBox = new QMessageBox(icon, title, fullMsg, QMessageBox::Ok, parent);
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->setWindowModality(Qt::NonModal);
            msgBox->show();
        };
        if (QThread::currentThread() == QApplication::instance()->thread()) {
            showFunc();
        } else {
            QMetaObject::invokeMethod(QApplication::instance(), showFunc, Qt::QueuedConnection);
        }
    }
}

QString ZLog::levelToString(Level level) const
{
    switch (level) {
    case DEBUG:    return "DEBUG";
    case INFO:     return "INFO";
    case QUESTION: return "QUESTION";
    case WARNING:  return "WARN";
    case CRITICAL: return "CRITICAL";
    default:       return "UNKNOWN";
    }
}

QString ZLog::formatLog(quint64 id, Level level, const QString &file, int line,
                        const QString &function, const QString &message) const
{
    QString result = m_format;
    result.replace("{time}", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    result.replace("{level}", levelToString(level));
    result.replace("{thread}", QString::number((quintptr)QThread::currentThread()));
    result.replace("{file}", QFileInfo(file).fileName());
    result.replace("{line}", QString::number(line));
    result.replace("{function}", function);
    result.replace("{message}", message);
    result.prepend(QString("[ID:%1] ").arg(id));
    return result;
}

bool ZLog::openLogFile()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
#ifdef Q_OS_WIN
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);

    std::wstring wpath = m_filePath.toStdWString();
    m_fileHandle = CreateFileW(wpath.c_str(),
                               GENERIC_WRITE,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);
    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        qCritical() << "ZLog: Cannot open log file" << m_filePath;
        return false;
    }
    SetFilePointer(m_fileHandle, 0, NULL, FILE_END);
    int fd = _open_osfhandle((intptr_t)m_fileHandle, _O_APPEND);
    if (fd == -1) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    FILE *fp = _fdopen(fd, "a");
    if (!fp) {
        _close(fd);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    if (!m_file.open(fp, QIODevice::WriteOnly | QIODevice::Append, QFileDevice::DontCloseHandle)) {
        fclose(fp);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }
#else
    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCritical() << "ZLog: Cannot open log file" << m_filePath;
        return false;
    }
#endif
    m_stream.setDevice(&m_file);
    return true;
}

void ZLog::rollFiles()
{
    m_stream.flush();
    m_file.close();
#ifdef Q_OS_WIN
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);
    m_fileHandle = INVALID_HANDLE_VALUE;
#endif

    QString basePath = m_filePath;
    for (int i = m_maxBackups - 1; i >= 0; --i) {
        QString oldName = (i == 0) ? basePath : basePath + "." + QString::number(i);
        QString newName = basePath + "." + QString::number(i + 1);
        if (QFile::exists(oldName)) {
            if (i == m_maxBackups - 1) {
                QFile::remove(oldName);
            } else {
                QFile::rename(oldName, newName);
            }
        }
    }

    openLogFile();
}

quint64 ZLog::generateId() { return ++m_idCounter; }

QList<QVariantMap> ZLog::parseLogFile(const QString &filePath)
{
    QList<QVariantMap> entries;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QVariantMap entry;
        if (line.startsWith('{')) {
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                entry["id"] = obj.value("id").toString();
                entry["time"] = obj.value("time").toString();
                entry["level"] = obj.value("level").toString();
                entry["thread"] = obj.value("thread").toString();
                entry["file"] = obj.value("file").toString();
                entry["line"] = obj.value("line").toInt();
                entry["function"] = obj.value("function").toString();
                entry["message"] = obj.value("message").toString();
                entries.append(entry);
                continue;
            }
        }
        // 兼容旧文本格式
        QRegularExpression re("^\\[ID:(\\d+)\\] (.+)$");
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            entry["id"] = match.captured(1);
            entry["raw"] = match.captured(2);
        } else {
            entry["raw"] = line;
        }
        entries.append(entry);
    }
    return entries;
}

// WinAPI 错误实现
void ZLog::logWinError(DWORD errorCode,
                       const QString &extraInfo,
                       const QString &overrideAppName,
                       QWidget *parent,
                       const QString &file,
                       int line,
                       const QString &function,
                       bool forcePopup,
                       bool forceNoPopup,
                       bool blocking)
{
    QString sysMsg;
#ifdef Q_OS_WIN
    wchar_t *buffer = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode,
        MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
        (LPWSTR)&buffer, 0, NULL);
    if (len > 0 && buffer) {
        sysMsg = QString::fromWCharArray(buffer, len).trimmed();
        LocalFree(buffer);
    } else {
        sysMsg = QString("未知错误 (0x%1)").arg(errorCode, 8, 16, QChar('0'));
    }
#else
    sysMsg = QString("错误码: %1 (WinAPI 信息仅 Windows 可用)").arg(errorCode);
#endif

    QString message = QString("错误码：%1 (0x%2)\n系统描述：%3")
                          .arg(errorCode)
                          .arg(errorCode, 8, 16, QChar('0'))
                          .arg(sysMsg);
    if (!extraInfo.isEmpty())
        message += "\n附加信息：" + extraInfo;

    write(CRITICAL,
          file.isEmpty() ? "WinAPI" : file,
          line,
          function.isEmpty() ? "logWinError" : function,
          message,
          forcePopup, forceNoPopup, blocking,
          parent ? parent : s_defaultPopupParent);

    if (!overrideAppName.isEmpty() && overrideAppName != m_appName) {
        auto showFunc = [overrideAppName, message, parent]() {
            QWidget *p = parent ? parent : s_defaultPopupParent;
            QMessageBox::critical(p, overrideAppName + " - 严重错误", message);
        };
        QMetaObject::invokeMethod(QApplication::instance(), showFunc,
                                  blocking ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
    }
}

// COM 错误实现
void ZLog::logComError(HRESULT hr,
                       const QString &extraInfo,
                       const QString &overrideAppName,
                       QWidget *parent,
                       const QString &file,
                       int line,
                       const QString &function,
                       bool forcePopup,
                       bool forceNoPopup,
                       bool blocking)
{
    QString comMsg;
#ifdef Q_OS_WIN
    try {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        comMsg = errMsg ? QString::fromWCharArray(errMsg)
                        : QString("未知 COM 错误 (0x%1)").arg((quint32)hr, 8, 16, QChar('0'));
    } catch (...) {
        comMsg = QString("未知 COM 错误 (0x%1)").arg((quint32)hr, 8, 16, QChar('0'));
    }
#else
    comMsg = QString("COM 错误 0x%1 (仅 Windows 可用)").arg((quint32)hr, 8, 16, QChar('0'));
#endif

    QString message = QString("COM 错误：0x%1\n描述：%2")
                          .arg((quint32)hr, 8, 16, QChar('0'))
                          .arg(comMsg);
    if (!extraInfo.isEmpty())
        message += "\n附加信息：" + extraInfo;

    write(CRITICAL,
          file.isEmpty() ? "COM" : file,
          line,
          function.isEmpty() ? "logComError" : function,
          message,
          forcePopup, forceNoPopup, blocking,
          parent ? parent : s_defaultPopupParent);

    if (!overrideAppName.isEmpty() && overrideAppName != m_appName) {
        auto showFunc = [overrideAppName, message, parent]() {
            QWidget *p = parent ? parent : s_defaultPopupParent;
            QMessageBox::critical(p, overrideAppName + " - 严重错误", message);
        };
        QMetaObject::invokeMethod(QApplication::instance(), showFunc,
                                  blocking ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
    }
}

// Qt 消息处理器
void ZLog::installQtMessageHandler()
{
    s_oldHandler = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        Level level;
        switch (type) {
        case QtDebugMsg:    level = DEBUG; break;
        case QtInfoMsg:     level = INFO; break;
        case QtWarningMsg:  level = WARNING; break;
        case QtCriticalMsg: level = CRITICAL; break;
        case QtFatalMsg:    level = CRITICAL; break;
        default:            level = INFO;
        }

        QString file = context.file ? QString::fromUtf8(context.file) : "unknown";
        int line = context.line;
        QString function = context.function ? QString::fromUtf8(context.function) : "unknown";

        instance()->doWrite(instance()->generateId(), level, file, line, function, msg);

        if (s_globalPopupEnabled && level == CRITICAL) {
            instance()->showPopup(CRITICAL, msg, false, s_defaultPopupParent);
        }

        if (s_oldHandler) s_oldHandler(type, context, msg);
    });
}

// 异常处理
static void zlogTerminateHandler()
{
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception &e) {
        ZLog::instance()->write(ZLog::CRITICAL, "terminate", 0, "terminateHandler",
                                QString("未处理的异常: %1").arg(e.what()),
                                false, false, true);
    } catch (...) {
        ZLog::instance()->write(ZLog::CRITICAL, "terminate", 0, "terminateHandler",
                                "未知的未处理异常",
                                false, false, true);
    }
}

void ZLog::installTerminateHandler()
{
    std::set_terminate(zlogTerminateHandler);
}