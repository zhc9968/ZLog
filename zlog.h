#ifndef ZLOG_H
#define ZLOG_H

#include <QObject>
#include <QMutex>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QMessageBox>
#include <QMetaObject>
#include <QApplication>
#include <QThread>
#include <QDebug>
#include <sstream>
#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ZLogViewer;

class ZLog : public QObject
{
    Q_OBJECT

public:
    enum Level {
        DEBUG,
        INFO,
        QUESTION,
        WARNING,
        CRITICAL
    };
    Q_ENUM(Level)

    enum FileRolling {
        NoRolling,
        SizeRolling,
        DailyRolling
    };

    static ZLog* instance();

    // 初始化（必须提供应用名称）
    void init(const QString &appName,
              const QString &logFilePath = "app.log",
              Level minLevel = DEBUG,
              FileRolling rolling = SizeRolling,
              qint64 maxSizeBytes = 10 * 1024 * 1024,
              int maxBackups = 5,
              bool consoleOutput = true,
              const QString &logFormat = "{time} [{level}] {thread} {file}:{line} - {function}: {message}");

    // 全局弹窗开关
    static void setGlobalPopupEnabled(bool enabled);
    static bool globalPopupEnabled();

    // 父窗口设置
    static void setDefaultPopupParent(QWidget *parent);
#ifdef Q_OS_WIN
    static void setDefaultPopupParent(HWND hwnd);
#endif
    static QWidget* defaultPopupParent();

    // 日志查看器控制
    static void setLogViewerEnabled(bool enabled);
    static bool isLogViewerEnabled();
    static void showLogViewer();
    static void hideLogViewer();
    static ZLogViewer* logViewer();

    void setMinLevel(Level level);
    void setConsoleOutput(bool enabled);
    void setFormat(const QString &format);

    // 底层写入（一般通过宏调用）
    void write(Level level, const QString &file, int line, const QString &function,
               const QString &message,
               bool forcePopup = false, bool forceNoPopup = false, bool blocking = true,
               QWidget *parent = nullptr
#ifdef Q_OS_WIN
               , HWND hwndParent = nullptr
#endif
               );

    // WinAPI 错误日志（支持弹窗控制、父窗口）
    void logWinError(DWORD errorCode,
                     const QString &extraInfo = {},
                     const QString &overrideAppName = {},
                     QWidget *parent = nullptr,
                     const QString &file = {},
                     int line = 0,
                     const QString &function = {},
                     bool forcePopup = false,
                     bool forceNoPopup = false,
                     bool blocking = true);

    // COM 错误日志
    void logComError(HRESULT hr,
                     const QString &extraInfo = {},
                     const QString &overrideAppName = {},
                     QWidget *parent = nullptr,
                     const QString &file = {},
                     int line = 0,
                     const QString &function = {},
                     bool forcePopup = false,
                     bool forceNoPopup = false,
                     bool blocking = true);

    // 接管 Qt 消息与崩溃
    static void installQtMessageHandler();
    static void installTerminateHandler();

    // 供日志查看器解析文件
    static QList<QVariantMap> parseLogFile(const QString &filePath);

signals:
signals:
    void newMessage(quint64 id, ZLog::Level level, const QString &message,
                    const QString &file, int line, const QString &function,
                    quintptr threadId);   // 增加线程ID参数

private:
    explicit ZLog(QObject *parent = nullptr);
    ~ZLog();
    Q_DISABLE_COPY(ZLog)

    void doWrite(quint64 id, Level level, const QString &file, int line,
                 const QString &function, const QString &message);
    void showPopup(Level level, const QString &message, bool blocking, QWidget *parent);
    QString levelToString(Level level) const;
    QString formatLog(quint64 id, Level level, const QString &file, int line,
                      const QString &function, const QString &message) const;
    bool openLogFile();
    void rollFiles();
    quint64 generateId();

    QMutex m_mutex;
    QFile m_file;
    QTextStream m_stream;
    QString m_appName;
    QString m_filePath;
    Level m_minLevel;
    FileRolling m_rolling;
    qint64 m_maxSize;
    int m_maxBackups;
    bool m_consoleOutput;
    QString m_format;

    std::atomic<quint64> m_idCounter{0};

    static ZLog *s_instance;
    static QtMessageHandler s_oldHandler;
    static bool s_globalPopupEnabled;
    static QWidget *s_defaultPopupParent;
    static bool s_logViewerEnabled;
    static ZLogViewer *s_logViewer;

#ifdef Q_OS_WIN
    HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
    static HWND s_defaultPopupHwnd;
#endif
};

// 流式写入辅助类（扩展了 WinAPI/COM 控制）
class ZLogStream {
public:
    ZLogStream(ZLog::Level level, const QString &file, int line, const QString &function)
        : m_level(level), m_file(file), m_line(line), m_function(function) {}

    ~ZLogStream() {
        if (m_winErrorCode != 0) {
            ZLog::instance()->logWinError(m_winErrorCode, m_winExtraInfo,
                                          QString(), m_parent, m_file, m_line, m_function,
                                          m_forcePopup, m_forceNoPopup, m_blocking);
        } else if (m_comError != 0) {
            ZLog::instance()->logComError(m_comError, m_comExtraInfo,
                                          QString(), m_parent, m_file, m_line, m_function,
                                          m_forcePopup, m_forceNoPopup, m_blocking);
        } else {
            ZLog::instance()->write(m_level, m_file, m_line, m_function,
                                    QString::fromStdString(m_stream.str()),
                                    m_forcePopup, m_forceNoPopup, m_blocking, m_parent);
        }
    }

    template<typename T>
    ZLogStream& operator<<(const T& value) {
        m_stream << value;
        return *this;
    }

    ZLogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        m_stream << manip;
        return *this;
    }

    // 控制方法
    ZLogStream& popup(bool enable = true) {
        m_forcePopup = enable;
        m_forceNoPopup = !enable;
        return *this;
    }
    ZLogStream& nopopup() { return popup(false); }
    ZLogStream& blocking(bool block = true) {
        m_blocking = block;
        return *this;
    }
    ZLogStream& parent(QWidget *widget) {
        m_parent = widget;
        return *this;
    }
#ifdef Q_OS_WIN
    ZLogStream& parent(HWND hwnd) {
        m_hwndParent = hwnd;
        return *this;
    }
#endif

    ZLogStream& winError(DWORD errorCode, const QString& extraInfo = {}) {
        m_winErrorCode = errorCode;
        m_winExtraInfo = extraInfo;
        return *this;
    }

    ZLogStream& comError(HRESULT hr, const QString& extraInfo = {}) {
        m_comError = hr;
        m_comExtraInfo = extraInfo;
        return *this;
    }

private:
    ZLog::Level m_level;
    QString m_file;
    int m_line;
    QString m_function;
    std::ostringstream m_stream;
    bool m_forcePopup = false;
    bool m_forceNoPopup = false;
    bool m_blocking = true;
    QWidget *m_parent = nullptr;
#ifdef Q_OS_WIN
    HWND m_hwndParent = nullptr;
#endif

    DWORD m_winErrorCode = 0;
    QString m_winExtraInfo;
    HRESULT m_comError = 0;
    QString m_comExtraInfo;
};

// 便捷宏
#define LOG_DEBUG()    ZLogStream(ZLog::DEBUG,    __FILE__, __LINE__, Q_FUNC_INFO)
#define LOG_INFO()     ZLogStream(ZLog::INFO,     __FILE__, __LINE__, Q_FUNC_INFO)
#define LOG_QUESTION() ZLogStream(ZLog::QUESTION, __FILE__, __LINE__, Q_FUNC_INFO)
#define LOG_WARNING()  ZLogStream(ZLog::WARNING,  __FILE__, __LINE__, Q_FUNC_INFO)
#define LOG_CRITICAL() ZLogStream(ZLog::CRITICAL, __FILE__, __LINE__, Q_FUNC_INFO)

#define LOG_WIN_ERROR(errorCode, extraInfo) \
ZLogStream(ZLog::CRITICAL, __FILE__, __LINE__, Q_FUNC_INFO).winError(errorCode, extraInfo)

#define LOG_WIN_ERROR_LAST(extraInfo) \
    ZLogStream(ZLog::CRITICAL, __FILE__, __LINE__, Q_FUNC_INFO).winError(GetLastError(), extraInfo)

#define LOG_COM_ERROR(hr, extraInfo) \
    ZLogStream(ZLog::CRITICAL, __FILE__, __LINE__, Q_FUNC_INFO).comError(hr, extraInfo)

#endif // ZLOG_H