### View API documentation here  
（Display method is incorrect）  
[View on GitHub Pages](https://zhc9968.github.io/ZLog/index-en.html)

# ZLog —— Qt Application Logging System

## Overview

ZLog is a full-featured logging module designed for Qt desktop applications. It provides leveled logging, file rolling, popup notifications, a graphical viewer, and system error integration. Developers only need to include two header files and call a few convenience macros to add powerful logging capabilities to their applications.

###block_list_start
###block_item_start
**Five Log Levels**  
DEBUG / INFO / QUESTION / WARNING / CRITICAL, covering everything from debugging details to critical errors
###block_item_end
###block_item_start
**Structured File Output**  
Each log entry is a single line of JSON, supporting automatic file rolling by size and backup retention
###block_item_end
###block_item_start
**Smart Popup System**  
Combines a global switch with per-message force/disable, supports modal and non-modal popups, and allows specifying a parent window
###block_item_end
###block_item_start
**Graphical Viewer**  
Real-time table, color coding, multi-condition filtering, import/export, right-click copy, and more out of the box
###block_item_end
###block_item_start
**System Error Integration**  
One-click recording of Windows API / COM errors with automatic translation into human-readable Chinese descriptions
###block_item_end
###block_item_start
**Crash Handling**  
Can install a Qt message handler and a `std::terminate` handler; unhandled exceptions are automatically written as CRITICAL log entries
###block_item_end
###block_list_end

---

## Integration Guide

### Requirements

###block_ul_start
- Qt 5.12 or higher (Qt 5.15+ recommended)
- C++11 capable compiler (MSVC 2017+, GCC 7+, Clang 5+)
- Windows platform: requires linking `user32.lib`, `ole32.lib` (needed for error translation)
###block_ul_end

### File List

###block_green_start
Simply add the following 4 files to your project to use ZLog. No external dependencies other than Qt.
###block_green_end

| File | Description |
|------|-------------|
| `zlog.h` | ZLog core class and convenience macro declarations |
| `zlog.cpp` | ZLog implementation |
| `zlogviewer.h` | Graphical log viewer declaration |
| `zlogviewer.cpp` | Viewer implementation |

### Integration Steps

#### 1. Copy the above 4 files into your project source directory.
#### 2. Add to your `.pro` file:
```qmake
SOURCES += zlog.cpp zlogviewer.cpp
HEADERS += zlog.h zlogviewer.h
```
If using CMake:
```cmake
target_sources(myapp PRIVATE zlog.cpp zlogviewer.cpp)
```
#### 3. Include the header files in source files where logging is needed:
```cpp
#include "zlog.h"
#include "zlogviewer.h"   // If you need the viewer
```

---

## Quick Start

```cpp
#include <QApplication>
#include "zlog.h"
#include "zlogviewer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. Initialize (must be called before any log output)
    ZLog::instance()->init(
        "OrderSystem",          // Application name
        "logs/app.log",         // Log file path
        ZLog::DEBUG,            // Minimum log level
        ZLog::SizeRolling,      // Rolling strategy
        5 * 1024 * 1024,        // Roll when file exceeds 5 MB
        3                       // Keep up to 3 backups
    );

    // 2. (Optional) Show built-in log viewer
    ZLog::showLogViewer();

    // 3. Use macros to start logging
    LOG_INFO() << "System started, version 1.2.0";
    LOG_DEBUG() << "Current user ID:" << 1001;

    return app.exec();
}
```

###block_green_start
After execution, the log file `logs/app.log` will continuously record in JSON line format; if the viewer is enabled, all logs will be displayed in real time in a colored table.
###block_green_end

---

## Core API Reference

### ZLog Class — The Logging Engine

Singleton class, obtain the global unique instance via `ZLog::instance()`. All configuration and writing operations are performed through this instance.

#### Initialization `init`

```cpp
void init(const QString &appName,
          const QString &logFilePath = "app.log",
          Level minLevel = DEBUG,
          FileRolling rolling = SizeRolling,
          qint64 maxSizeBytes = 10 * 1024 * 1024,
          int maxBackups = 5,
          bool consoleOutput = true,
          const QString &logFormat = "{time} [{level}] {thread} {file}:{line} - {function}: {message}");
```

###block_orange_start
Must be called early in the program, before any log output, and only once. Repeated calls will reopen the log file.
###block_orange_end

| Parameter | Type | Description |
|-----------|------|-------------|
| `appName` | `QString` | Application name, used in popup title (displayed as `"appName - Level"`) |
| `logFilePath` | `QString` | Full path to the log file; non-existent directories are created automatically |
| `minLevel` | `Level` | Minimum logging level; entries below this level are completely discarded, neither written to file nor shown in popups |
| `rolling` | `FileRolling` | File rolling strategy, optional `NoRolling` (no rolling) or `SizeRolling` (roll by size) |
| `maxSizeBytes` | `qint64` | When strategy is `SizeRolling`, the file size in bytes that triggers rolling |
| `maxBackups` | `int` | Maximum number of backup files to retain; the oldest are deleted when exceeded |
| `consoleOutput` | `bool` | Whether to also output to the Qt debug console (`qDebug()` stream) |
| `logFormat` | `QString` | Console output format, supports placeholders (see table below) |

**Supported Placeholders**

| Placeholder | Replaced with |
|-------------|---------------|
| `{time}` | Timestamp, format `yyyy-MM-dd hh:mm:ss.zzz` |
| `{level}` | Level text (`DEBUG`, `INFO`, etc.) |
| `{thread}` | Thread ID (decimal digit string) |
| `{file}` | Source file name (without path) |
| `{line}` | Line number |
| `{function}` | Function signature |
| `{message}` | Log message body |

###block_orange_start
Regardless of how `logFormat` is configured, the console output will always be automatically prefixed with `[ID:<incrementing number>]`. This behavior cannot be configured.
###block_orange_end

#### Enumerations

##### `enum ZLog::Level`
| Enumerator | Value | Meaning | Default popup behavior |
|------------|-------|---------|-------------------------|
| `DEBUG` | 0 | Development debugging details | No popup |
| `INFO` | 1 | General flow information | Popup if global switch is on |
| `QUESTION` | 2 | Queries that need attention | Popup if global switch is on |
| `WARNING` | 3 | Potential problems not affecting core functionality | Popup if global switch is on |
| `CRITICAL` | 4 | Severe errors or imminent crash | Popup if global switch is on |

##### `enum ZLog::FileRolling`
| Enumerator | Description |
|------------|-------------|
| `NoRolling` | No rolling; all logs written to the same file |
| `SizeRolling` | Roll by size; when exceeding `maxSizeBytes`, a new file is created and old files are renamed to `.1`, `.2`, etc. |
| `DailyRolling` | **Defined but not implemented**, actual effect is the same as `NoRolling` |

###block_red_start
The `DailyRolling` enumerator exists in the code, but the current version does not implement date-based rolling logic. If passed, logs will not switch files by day. Please avoid using it for now.
###block_red_end

#### Global Configuration Methods

| Static Method | Description |
|---------------|-------------|
| `setGlobalPopupEnabled(bool enabled)` | Global popup master switch. `false` disables all popups (except those forced per message) |
| `globalPopupEnabled()` | Returns the current global switch state |
| `setDefaultPopupParent(QWidget *parent)` | Sets the default parent window; popups will be centered on this window. Pass `nullptr` to restore independent top-level windows |
| `defaultPopupParent()` | Gets the current default parent window |

#### Viewer Control Methods

| Static Method | Description |
|---------------|-------------|
| `setLogViewerEnabled(bool enabled)` | Enable/disable the built-in viewer. When disabled, `showLogViewer()` has no effect |
| `isLogViewerEnabled()` | Whether the viewer is allowed to be used |
| `showLogViewer()` | If enabled, creates and shows the viewer window. If the window already exists, activates and brings it to the front |
| `hideLogViewer()` | Hides the viewer window (does not destroy it) |
| `logViewer()` | Returns the viewer instance pointer, may be `nullptr` |

#### Dynamic Settings

| Instance Method | Description |
|-----------------|-------------|
| `setMinLevel(Level level)` | Change the minimum logging level at runtime, useful for temporary filtering |
| `setConsoleOutput(bool enabled)` | Toggle console output at runtime |
| `setFormat(const QString &format)` | Modify the console output format at runtime (the ID prefix is still automatically added) |

#### Low-level Write `write`

###block_green_start
Usually you do not need to call this method directly; the convenience macros automatically capture source location and support chainable control. The full signature is listed here for advanced customization needs.
###block_green_end

```cpp
void write(Level level,
           const QString &file, int line,
           const QString &function,
           const QString &message,
           bool forcePopup = false,
           bool forceNoPopup = false,
           bool blocking = true,
           QWidget *parent = nullptr);
```

| Parameter | Description |
|-----------|-------------|
| `level` | Log level |
| `file`, `line`, `function` | Source location, automatically captured by macros; must be provided manually when calling directly |
| `message` | Log body |
| `forcePopup` | `true` forces a popup dialog, ignoring the global switch |
| `forceNoPopup` | `true` forces no popup, highest priority |
| `blocking` | `true` for modal (blocks the thread), `false` for non-modal |
| `parent` | Parent widget for the popup; `nullptr` uses the global default parent |

**Popup decision order**:  
`forceNoPopup` (yes → no popup) → `forcePopup` (yes → popup) → level is `DEBUG` (no popup) → global switch decides.

###block_red_start
The `write` method signature contains an `HWND hwndParent` parameter (Windows only), but this parameter is **completely unused** in the function body. Always use `QWidget* parent` to specify the parent window.
###block_red_end

#### System Error Recording

###block_red_start
**Convenient** calling methods are described below under `Convenience Macros & Chaining`.
###block_red_end

##### `logWinError`
```cpp
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
```
Automatically retrieves the system description (in Chinese) corresponding to the Windows error code and generates a `CRITICAL` level log entry.  
Example generated message:
```
Error code: 2 (0x00000002)
System description: The system cannot find the file specified.
Additional info: Failed to read configuration file
```

##### `logComError`
```cpp
void logComError(HRESULT hr,
                 const QString &extraInfo = {},
                 // ... other parameters same as logWinError
                 );
```
Uses `_com_error` to extract the COM error description, level `CRITICAL`.

###block_orange_start
On non-Windows platforms, calling these two methods will show a placeholder text like "WinAPI information only available on Windows".
###block_orange_end

#### Advanced Integration Methods

| Static Method | Description |
|---------------|-------------|
| `installQtMessageHandler()` | Intercepts Qt messages: `qDebug()`→DEBUG, `qInfo()`→INFO, `qWarning()`→WARNING, `qCritical()`→CRITICAL (and shows popup) |
| `installTerminateHandler()` | Installs a `std::terminate` handler; uncaught C++ exceptions are logged as CRITICAL and a popup is shown (blocking) |
| `parseLogFile(filePath)` | Parses a JSON line log file, returns `QList<QVariantMap>`, each containing `id, time, level, thread, file, line, function, message` |

#### Signals

`void newMessage(quint64 id, Level level, const QString &message, const QString &file, int line, const QString &function, quintptr threadId)`  
Emitted every time a log entry is successfully written; the graphical viewer relies on this signal for real-time updates.

---

### ZLogViewer Class — Graphical Viewer

###block_green_start
The viewer is an independent `QWidget` window that can be opened/closed freely at runtime without affecting log recording.
###block_green_end

#### Public Methods

| Method | Description |
|--------|-------------|
| `ZLogViewer(QWidget *parent = nullptr)` | Constructor, automatically connects to the `newMessage` signal |
| `~ZLogViewer()` | Emits `closed()` signal before destruction |
| `loadFromFile(filePath)` | Loads a JSON line log file into the table display |
| `clear()` | Clears all rows from the current table |
| `showTop()` | Shows and brings the window to the top |
| `setAutoScroll(bool)` | Whether to automatically scroll to the bottom when new logs arrive |
| `autoScroll()` | Returns the auto-scroll state |
| `exportToFile(filePath, format)` | Exports currently filtered logs. `format` supports `"txt"` or `"csv"` |
| `importFromFile(filePath)` | Imports logs directly from a file, without showing a dialog |

#### Signals

`void closed()` — Emitted when the user closes the viewer window.

#### Built-in UI Features

The viewer provides a fully interactive interface with no extra coding required:

###block_list_start
###block_item_start
**Color-coded table**  
8 columns: ID, Time, Level, Thread, File, Line, Function, Message; levels distinguished by color
###block_item_end
###block_item_start
**Filter toolbar**  
Text search (supports regular expressions), level dropdown filter
###block_item_end
###block_item_start
**Action buttons**  
Import, Export (txt/csv/json), Clear, Auto-scroll toggle
###block_item_end
###block_item_start
**Right-click menu**  
Copy current cell, Copy entire row, Delete selected row, View details
###block_item_end
###block_item_start
**Detail popup**  
Non-modal window, shows full row data, raw JSON, supports saving individual entry
###block_item_end
###block_list_end

---

## Convenience Macros & Chaining

Core recording method: macros automatically capture file name, line number, function name, return a temporary object, concatenate message with `<<`, and can be chained to control popups or record special errors.

### Basic Macros

| Macro | Corresponding Level |
|-------|---------------------|
| `LOG_DEBUG()` | DEBUG |
| `LOG_INFO()` | INFO |
| `LOG_QUESTION()` | QUESTION |
| `LOG_WARNING()` | WARNING |
| `LOG_CRITICAL()` | CRITICAL |

### Chainable Control Methods

| Method | Description |
|--------|-------------|
| `.popup(bool enable = true)` | Forces a popup message box |
| `.nopopup()` | Forces no popup, highest priority |
| `.blocking(bool block = true)` | When `true`, the popup is modal (blocks the thread); `false` makes it non-modal |
| `.parent(QWidget *widget)` | Specifies the parent widget for this popup (overrides the global setting) |
| `.winError(DWORD code, const QString& extra)` | Converts this log entry into a Windows error record, ignores the `<<` content, level becomes CRITICAL |
| `.comError(HRESULT hr, const QString& extra)` | Converts into a COM error record, same behavior |

### Error Shortcut Macros

| Macro | Description |
|-------|-------------|
| `LOG_WIN_ERROR(code, extraInfo)` | Directly records a Windows error code |
| `LOG_WIN_ERROR_LAST(extraInfo)` | Automatically calls `GetLastError()` to obtain the error code |
| `LOG_COM_ERROR(hr, extraInfo)` | Directly records a COM error code |

###block_green_start
Error shortcut macros also **support** chaining of `popup`, `blocking`, and `parent`.
###block_green_end

### Chaining Examples

```cpp
// Ordinary log
LOG_INFO() << "User" << name << "logged in successfully";

// Force popup and block (user must acknowledge to continue)
LOG_CRITICAL().popup(true).blocking(true) << "Database connection lost!";

// Silent warning (no popup, only file and console)
LOG_WARNING().nopopup() << "Disk space below 10%";

// Non-blocking notification
LOG_INFO().popup().blocking(false) << "A new version is available";

// Record a Windows error
if (!DeleteFile(L"...")) {
    LOG_WIN_ERROR_LAST("Failed to delete temporary file");
}

// Record a COM error
HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
if (FAILED(hr)) {
    LOG_COM_ERROR(hr, "COM initialization failed");
}
```

---

## Output Characteristics

### Log File Format
Each log entry is one line of compact JSON:
```json
{"id":"1","time":"2025-03-15 14:30:22.123","level":"INFO","thread":"14073456","file":"main.cpp","line":10,"function":"main","message":"Program started"}
```

### Console Format
Default template output example:
```
[ID:1] 2025-03-15 14:30:22.123 [INFO] 14073456 main.cpp:10 - main: Program started
```
###block_orange_start
Regardless of the custom template setting, `[ID:n]` will be automatically prepended at the beginning of each line and cannot be removed.
###block_orange_end

---

## Known Limitations

###block_red_start
**1. `DailyRolling` enumerator is defined but not implemented**  
The code defines the `ZLog::DailyRolling` enumerator, and `init()` can accept it, but the internal logic contains no code for date-based rolling. Using this value results in behavior identical to `NoRolling`; log files will not switch.

**2. Windows HWND parent window parameter is ignored**  
Methods like `write()`, `logWinError()`, `logComError()` accept an `HWND hwndParent` parameter on Windows, but this parameter is never used in the function body. All popup parent window settings only take effect through `QWidget* parent`. Please ignore the `HWND` parameter and always use the `QWidget` version.

**3. Console output forces an ID prefix**  
The `logFormat` parameter of `init` allows customizing the console output format, but the actual code forcibly prepends `[ID:sequence number]` after formatting with `result.prepend(QString("[ID:%1] ").arg(id))`. Therefore all console log lines will start with `[ID:n]`. This behavior cannot be turned off or altered through configuration.
###block_red_end

---

## Troubleshooting

| Problem | Possible Cause & Solution |
|---------|---------------------------|
| Popup does not appear | 1. Global popup is disabled 2. `.nopopup()` was used 3. Level is DEBUG |
| Log viewer cannot be opened | Check if `setLogViewerEnabled(false)` was called; ensure `zlogviewer.cpp` is correctly compiled and linked |
| Windows error description shows "Only available on Windows" | Called `logWinError` or `logComError` on a non-Windows platform, this is expected |
| File rolling does not happen | Confirm strategy is `SizeRolling` and the file size has exceeded the configured threshold |
| Console format does not take effect | Check placeholder spelling. Note that the ID prefix is forcibly added and cannot be removed |
| Multithreading popup causes crash | Popup internally uses `QMetaObject::invokeMethod` to dispatch to the main thread, but still avoid creating QWidgets in non-GUI threads |

---

## Full Usage Example

```cpp
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include "zlog.h"
#include "zlogviewer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initialization
    ZLog::instance()->init("ZLogDemo", "logs/demo.log",
                           ZLog::DEBUG, ZLog::SizeRolling,
                           1 * 1024 * 1024, 3, true);

    // Install crash and Qt message handlers
    ZLog::installQtMessageHandler();
    ZLog::installTerminateHandler();

    // Create main window
    QMainWindow w;
    w.setWindowTitle("ZLog Full Demo");
    QWidget central;
    QPushButton btnInfo("Log INFO"), btnWarn("Silent WARNING"),
                btnCrit("Blocking CRITICAL"), btnWin("Simulate Win Error"),
                btnViewer("Show Viewer");
    QVBoxLayout layout(&central);
    layout.addWidget(&btnInfo); layout.addWidget(&btnWarn);
    layout.addWidget(&btnCrit); layout.addWidget(&btnWin);
    layout.addWidget(&btnViewer);
    w.setCentralWidget(&central);
    w.show();

    // Set popup parent window
    ZLog::setDefaultPopupParent(&w);

    // Show viewer
    ZLog::showLogViewer();

    // Connect signals
    QObject::connect(&btnInfo, &QPushButton::clicked, []() {
        LOG_INFO() << "INFO button clicked";
    });
    QObject::connect(&btnWarn, &QPushButton::clicked, []() {
        LOG_WARNING().nopopup() << "Disk usage over 90%, but auto-cleaned";
    });
    QObject::connect(&btnCrit, &QPushButton::clicked, []() {
        LOG_CRITICAL().popup(true).blocking(true) << "Payment gateway returned a fatal error!";
        LOG_INFO() << "User acknowledged the critical error popup";   // Executes after popup is closed
    });
    QObject::connect(&btnWin, &QPushButton::clicked, []() {
        DeleteFile(L"c:\\nonexistent_file.txt");
        LOG_WIN_ERROR_LAST("Attempted to delete a non-existent file");
    });
    QObject::connect(&btnViewer, &QPushButton::clicked, []() {
        ZLog::showLogViewer();
    });

    LOG_INFO() << "Application started successfully";

    return app.exec();
}
```

###block_green_start
After running this example:
- `logs/demo.log` continuously records all logs in JSON line format.
- The console outputs color-coded logs with an `[ID:n]` prefix (requires running inside Qt Creator).
- The viewer refreshes in real time, highlighting different levels in different colors.
- Clicking the "Silent WARNING" button produces no dialog.
- Clicking the "Blocking CRITICAL" button shows a modal error box; the program waits until the user confirms.
- The "Simulate Win Error" button automatically translates the system error code into a Chinese description and logs it.
###block_green_end

---

*ZLog aims to make logging in Qt desktop applications as simple as `qDebug()`, while providing enterprise-grade features and a friendly visual interface.*
