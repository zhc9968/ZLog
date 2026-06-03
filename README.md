### 在此处查看API文档显示方式不正确
[在GitHub Pages查看](https://zhc9968.github.io/ZLog/) 

# ZLog —— Qt 应用程序日志系统

## 概述

ZLog 是为 Qt 桌面应用设计的全功能日志模块，提供分级记录、文件滚动、弹窗通知、图形化查看器和系统错误集成。开发者只需引入两个头文件，调用少量便捷宏，即可为应用添加强大的日志能力。

###block_list_start
###block_item_start
**五级日志**  
DEBUG / INFO / QUESTION / WARNING / CRITICAL，覆盖从调试到严重错误的所有场景
###block_item_end
###block_item_start
**结构化文件输出**  
每条日志为一行 JSON，支持按文件大小自动滚动和备份保留
###block_item_end
###block_item_start
**智能弹窗系统**  
全局开关与单次强制/禁止结合，支持模态与非模态，可指定父窗口
###block_item_end
###block_item_start
**图形化查看器**  
实时表格、颜色标记、多条件过滤、导入导出、右键复制等开箱即用
###block_item_end
###block_item_start
**系统错误集成**  
一键记录 Windows API / COM 错误并自动翻译为可读中文描述
###block_item_end
###block_item_start
**崩溃接管**  
可安装 Qt 消息处理器和 `std::terminate` 处理器，未处理异常自动写入 CRITICAL 日志
###block_item_end
###block_list_end

---

## 集成指南

### 环境要求

###block_ul_start
- Qt 5.12 或更高版本（推荐 Qt 5.15+）
- 支持 C++11 的编译器（MSVC 2017+、GCC 7+、Clang 5+）
- Windows 平台：需链接 `user32.lib`、`ole32.lib`（错误翻译所需）
###block_ul_end

### 文件清单

###block_green_start
只需将以下 4 个文件加入项目即可使用 ZLog，无其他外部依赖（除 Qt 外）。
###block_green_end

| 文件 | 说明 |
|------|------|
| `zlog.h` | ZLog 核心类与便捷宏声明 |
| `zlog.cpp` | ZLog 实现 |
| `zlogviewer.h` | 图形化日志查看器声明 |
| `zlogviewer.cpp` | 查看器实现 |

### 集成步骤

#### 1.将上述 4 个文件复制到项目源码目录。
#### 2.在 `.pro` 文件中添加：
```qmake
SOURCES += zlog.cpp zlogviewer.cpp
HEADERS += zlog.h zlogviewer.h
```
若使用 CMake：
```cmake
target_sources(myapp PRIVATE zlog.cpp zlogviewer.cpp)
```
#### 3.在需要记录日志的源文件中包含头文件：
```cpp
#include "zlog.h"
#include "zlogviewer.h"   // 若需使用查看器
```

---

## 快速开始

```cpp
#include <QApplication>
#include "zlog.h"
#include "zlogviewer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. 初始化（必须在任何日志输出前调用）
    ZLog::instance()->init(
        "订单系统",            // 应用名称
        "logs/app.log",        // 日志文件路径
        ZLog::DEBUG,           // 最低记录级别
        ZLog::SizeRolling,     // 滚动策略
        5 * 1024 * 1024,       // 文件超过 5MB 时滚动
        3                      // 最多保留 3 个备份
    );

    // 2. （可选）显示内置日志查看器
    ZLog::showLogViewer();

    // 3. 使用宏开始记录
    LOG_INFO() << "系统启动，版本 1.2.0";
    LOG_DEBUG() << "当前用户 ID:" << 1001;

    return app.exec();
}
```

###block_green_start
执行后，日志文件 `logs/app.log` 将持续以 JSON 行格式记录；若开启了查看器，所有日志将实时显示在彩色表格中。
###block_green_end

---

## 核心 API 参考

### ZLog 类 —— 日志引擎

单例类，通过 `ZLog::instance()` 获取全局唯一实例。所有设置和写入操作均通过该实例进行。

#### 初始化 `init`

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
必须在程序启动早期、任何日志输出前调用，且只能调用一次。重复调用将重新打开日志文件。
###block_orange_end

| 参数 | 类型 | 说明 |
|------|------|------|
| `appName` | `QString` | 应用名称，用于弹窗标题（显示为 `"appName - 级别"`） |
| `logFilePath` | `QString` | 日志文件完整路径，不存在的目录会被自动创建 |
| `minLevel` | `Level` | 最低记录级别，低于此级别的日志将被完全丢弃，不写文件也不弹窗 |
| `rolling` | `FileRolling` | 文件滚动策略，可选 `NoRolling`（不滚动）或 `SizeRolling`（按大小滚动） |
| `maxSizeBytes` | `qint64` | 当策略为 `SizeRolling` 时，触发滚动的文件字节数 |
| `maxBackups` | `int` | 保留的备份文件最大数量，超出后最旧的被删除 |
| `consoleOutput` | `bool` | 是否同步输出到 Qt 调试控制台（`qDebug()` 流） |
| `logFormat` | `QString` | 控制台输出格式，支持占位符（见下表） |

**支持的占位符**

| 占位符 | 替换内容 |
|--------|----------|
| `{time}` | 时间戳，格式 `yyyy-MM-dd hh:mm:ss.zzz` |
| `{level}` | 级别文本（`DEBUG`、`INFO` 等） |
| `{thread}` | 线程 ID（十进制数字字符串） |
| `{file}` | 源文件名（不含路径） |
| `{line}` | 行号 |
| `{function}` | 函数签名 |
| `{message}` | 日志消息正文 |

###block_orange_start
无论 `logFormat` 如何设置，控制台实际输出的最前面始终会自动添加 `[ID:<递增序号>]` 前缀，该行为不可配置。
###block_orange_end

#### 枚举定义

##### `enum ZLog::Level`
| 枚举值 | 数值 | 含义 | 默认弹窗行为 |
|--------|------|------|-------------|
| `DEBUG` | 0 | 开发调试细节 | 不弹窗 |
| `INFO` | 1 | 一般流程信息 | 若全局开关打开则弹窗 |
| `QUESTION` | 2 | 需要留意的询问 | 若全局开关打开则弹窗 |
| `WARNING` | 3 | 可能存在问题但不影响核心功能 | 若全局开关打开则弹窗 |
| `CRITICAL` | 4 | 严重错误或即将崩溃 | 若全局开关打开则弹窗 |

##### `enum ZLog::FileRolling`
| 枚举值 | 说明 |
|--------|------|
| `NoRolling` | 不滚动，所有日志写入同一个文件 |
| `SizeRolling` | 按大小滚动，超过 `maxSizeBytes` 时生成新文件，旧文件依次重命名为 `.1`、`.2` 等 |
| `DailyRolling` | **已定义但未实现**，实际效果等同于 `NoRolling` |

###block_red_start
`DailyRolling` 枚举值存在于代码中，但当前版本并未实现按日期滚动的逻辑。若传入该值，日志不会按日切换文件，请暂时避免使用。
###block_red_end

#### 全局配置方法

| 静态方法 | 说明 |
|----------|------|
| `setGlobalPopupEnabled(bool enabled)` | 全局弹窗总开关。`false` 将禁用所有弹窗（除单条强制要求外） |
| `globalPopupEnabled()` | 返回当前全局开关状态 |
| `setDefaultPopupParent(QWidget *parent)` | 设置默认父窗口，弹窗将以此窗口为中心。传 `nullptr` 恢复为独立顶层窗口 |
| `defaultPopupParent()` | 获取当前默认父窗口 |

#### 查看器控制方法

| 静态方法 | 说明 |
|----------|------|
| `setLogViewerEnabled(bool enabled)` | 启用/禁用内置查看器。禁用后 `showLogViewer()` 无效果 |
| `isLogViewerEnabled()` | 查看器是否允许使用 |
| `showLogViewer()` | 若已启用，则创建并显示查看器窗口。若窗口已存在则激活置顶 |
| `hideLogViewer()` | 隐藏查看器窗口（不销毁） |
| `logViewer()` | 返回查看器实例指针，可能为 `nullptr` |

#### 动态设置方法

| 实例方法 | 说明 |
|----------|------|
| `setMinLevel(Level level)` | 运行时修改最低记录级别，可用于临时过滤日志 |
| `setConsoleOutput(bool enabled)` | 运行时开关控制台输出 |
| `setFormat(const QString &format)` | 运行时修改控制台输出格式（ID 前缀仍会自动添加） |

#### 底层写入 `write`

###block_green_start
通常不需要直接调用该方法，便捷宏已自动填充源码位置并支持链式控制。此处列出完整签名以满足高级定制需求。
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

| 参数 | 说明 |
|------|------|
| `level` | 日志级别 |
| `file`、`line`、`function` | 源码位置，宏自动捕获，手动调用时需自行传递 |
| `message` | 日志正文 |
| `forcePopup` | `true` 强制弹出对话框，无视全局开关 |
| `forceNoPopup` | `true` 强制不弹窗，优先级最高 |
| `blocking` | `true` 为模态（阻塞线程），`false` 为非模态 |
| `parent` | 弹窗父窗口，`nullptr` 使用全局默认父窗口 |

**弹窗决策顺序**：  
`forceNoPopup`（是 → 不弹） → `forcePopup`（是 → 弹） → 级别为 `DEBUG`（不弹） → 全局开关决定。

###block_red_start
`write` 方法签名中存在 `HWND hwndParent` 参数（仅在 Windows 下），但该参数在函数体内**完全未被使用**。指定父窗口请始终使用 `QWidget* parent`。
###block_red_end

#### 系统错误记录

###block_red_start
**简便**调用方法见下文`便捷宏与链式调用`
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
自动获取 Windows 错误码对应的系统描述（中文），生成级别为 `CRITICAL` 的日志。  
产生的消息格式示例：
```
错误码：2 (0x00000002)
系统描述：系统找不到指定的文件。
附加信息：读取配置文件失败
```

##### `logComError`
```cpp
void logComError(HRESULT hr,
                 const QString &extraInfo = {},
                 // ...其余参数同 logWinError
                 );
```
利用 `_com_error` 提取 COM 错误描述，级别为 `CRITICAL`。

###block_orange_start
在非 Windows 平台调用这两个方法，错误描述将显示为“WinAPI 信息仅 Windows 可用”或类似占位文本。
###block_orange_end

#### 高级集成方法

| 静态方法 | 说明 |
|----------|------|
| `installQtMessageHandler()` | 接管 Qt 消息：`qDebug()`→DEBUG, `qInfo()`→INFO, `qWarning()`→WARNING, `qCritical()`→CRITICAL（并弹窗） |
| `installTerminateHandler()` | 安装 `std::terminate` 处理器，未捕获的 C++ 异常将被记录为 CRITICAL 日志并弹窗（阻塞） |
| `parseLogFile(filePath)` | 解析 JSON 行格式日志文件，返回 `QList<QVariantMap>`，每条包含 `id, time, level, thread, file, line, function, message` |

#### 信号

`void newMessage(quint64 id, Level level, const QString &message, const QString &file, int line, const QString &function, quintptr threadId)`  
每成功写入一条日志时触发，图形查看器依赖此信号实时更新。

---

### ZLogViewer 类 —— 图形化查看器

###block_green_start
查看器是一个独立的 `QWidget` 窗口，可在程序运行时随意打开/关闭，不影响日志记录。
###block_green_end

#### 公共方法

| 方法 | 说明 |
|------|------|
| `ZLogViewer(QWidget *parent = nullptr)` | 构造函数，自动连接 `newMessage` 信号 |
| `~ZLogViewer()` | 销毁前发射 `closed()` 信号 |
| `loadFromFile(filePath)` | 加载 JSON 行日志文件到表格显示 |
| `clear()` | 清空当前表格所有行 |
| `showTop()` | 显示并置顶窗口 |
| `setAutoScroll(bool)` | 新日志到来时是否自动滚动到最底部 |
| `autoScroll()` | 返回自动滚动状态 |
| `exportToFile(filePath, format)` | 导出当前过滤后的日志。`format` 支持 `"txt"` 或 `"csv"` |
| `importFromFile(filePath)` | 直接从文件导入日志，不弹对话框 |

#### 信号

`void closed()` —— 当用户关闭查看器窗口时发射。

#### 内置界面功能

查看器提供了完整的交互界面，无需额外编码：

###block_list_start
###block_item_start
**彩色表格**  
ID、时间、级别、线程、文件、行、函数、消息共 8 列，级别以颜色区分
###block_item_end
###block_item_start
**过滤工具栏**  
文本搜索（支持正则）、级别下拉筛选
###block_item_end
###block_item_start
**操作按钮**  
导入、导出（txt/csv/json）、清空、自动滚动开关
###block_item_end
###block_item_start
**右键菜单**  
复制当前列、复制整行、删除选中行、查看详情
###block_item_end
###block_item_start
**详情弹窗**  
非模态窗口，展示完整行数据、JSON 原文，支持单条保存
###block_item_end
###block_list_end

---

## 便捷宏与链式调用

核心记录方式：宏自动捕获文件名、行号、函数名，返回临时对象，通过 `<<` 拼接消息，并可链式调用控制弹窗或记录特殊错误。

### 基础宏

| 宏 | 对应级别 |
|----|----------|
| `LOG_DEBUG()` | DEBUG |
| `LOG_INFO()` | INFO |
| `LOG_QUESTION()` | QUESTION |
| `LOG_WARNING()` | WARNING |
| `LOG_CRITICAL()` | CRITICAL |

### 链式控制方法

| 方法 | 说明 |
|------|------|
| `.popup(bool enable = true)` | 强制弹出消息框 |
| `.nopopup()` | 强制不弹窗，优先级最高 |
| `.blocking(bool block = true)` | 设为 `true` 时弹窗为模态（阻塞线程），`false` 为非模态 |
| `.parent(QWidget *widget)` | 指定本次弹窗的父窗口（覆盖全局设置） |
| `.winError(DWORD code, const QString& extra)` | 将本次日志转为 Windows 错误记录，忽略前面 `<<` 的内容，级别变为 CRITICAL |
| `.comError(HRESULT hr, const QString& extra)` | 转为 COM 错误记录，行为同上 |

### 错误快捷宏

| 宏 | 说明 |
|----|------|
| `LOG_WIN_ERROR(code, extraInfo)` | 直接记录 Windows 错误码 |
| `LOG_WIN_ERROR_LAST(extraInfo)` | 自动调用 `GetLastError()` 获取错误码 |
| `LOG_COM_ERROR(hr, extraInfo)` | 直接记录 COM 错误码 |

###block_green_start
错误快捷宏同样**支持**`popup`、`blocking`和`parent`的链式设置。
###block_green_end

### 链式调用示例

```cpp
// 普通日志
LOG_INFO() << "用户" << name << "登录成功";

// 强制弹窗且阻塞（用户必须确认才能继续）
LOG_CRITICAL().popup(true).blocking(true) << "数据库连接丢失！";

// 静默警告（不弹窗，仅写文件和控制台）
LOG_WARNING().nopopup() << "磁盘空间低于 10%";

// 非阻塞提示
LOG_INFO().popup().blocking(false) << "有新版本可用";

// 记录 Windows 错误
if (!DeleteFile(L"...")) {
    LOG_WIN_ERROR_LAST("删除临时文件失败");
}

// 记录 COM 错误
HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
if (FAILED(hr)) {
    LOG_COM_ERROR(hr, "COM 初始化失败");
}
```

---

## 输出特性说明

### 日志文件格式
每条日志为一行紧凑 JSON：
```json
{"id":"1","time":"2025-03-15 14:30:22.123","level":"INFO","thread":"14073456","file":"main.cpp","line":10,"function":"main","message":"程序启动"}
```

### 控制台格式
默认模板输出示例：
```
[ID:1] 2025-03-15 14:30:22.123 [INFO] 14073456 main.cpp:10 - main: 程序启动
```
###block_orange_start
无论自定义模板如何设置，行首的 `[ID:n]` 都会自动追加，不可移除。
###block_orange_end

---

## 已知限制

###block_red_start
**1. `DailyRolling` 枚举已定义但未实现**  
代码中定义了 `ZLog::DailyRolling` 枚举值，且 `init()` 可以接收该参数，但内部逻辑中完全没有处理按日期滚动的代码。若使用该值，实际效果等同于 `NoRolling`，日志文件不会切换。

**2. Windows HWND 父窗口参数无效**  
`write()`、`logWinError()`、`logComError()` 等方法在 Windows 下额外接收 `HWND hwndParent` 参数，但这些参数在函数体内并未被使用。所有弹窗的父窗口设置仅通过 `QWidget* parent` 生效。请忽略 `HWND` 参数，始终使用 `QWidget` 版本。

**3. 控制台输出强制 ID 前缀**  
`init` 的 `logFormat` 参数允许自定义控制台输出格式，但实际代码会在格式化完成后强制执行 `result.prepend(QString("[ID:%1] ").arg(id))`，因此所有控制台日志最前面都会带上 `[ID:序号]`。该行为无法通过配置关闭或修改。
###block_red_end

---

## 故障排除

| 问题现象 | 可能原因及解决方法 |
|----------|-------------------|
| 弹窗不显示 | 1. 全局弹窗被关闭 2. 使用了 `.nopopup()` 3. 级别为 DEBUG |
| 日志查看器无法打开 | 检查是否调用过 `setLogViewerEnabled(false)`；确认 `zlogviewer.cpp` 被正确编译链接 |
| Windows 错误描述显示“仅 Windows 可用” | 在非 Windows 平台调用了 `logWinError` 或 `logComError`，此为预期行为 |
| 文件滚动未发生 | 确认策略为 `SizeRolling`，且文件大小已超过设定阈值 |
| 控制台格式不生效 | 检查占位符拼写。注意 ID 前缀是强制添加的，无法去掉 |
| 多线程弹窗导致崩溃 | 弹窗内部已通过 `QMetaObject::invokeMethod` 调度到主线程，但仍应避免在非 GUI 线程创建 QWidget |

---

## 完整使用用例

```cpp
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include "zlog.h"
#include "zlogviewer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 初始化
    ZLog::instance()->init("ZLogDemo", "logs/demo.log",
                           ZLog::DEBUG, ZLog::SizeRolling,
                           1 * 1024 * 1024, 3, true);

    // 安装崩溃和 Qt 消息接管
    ZLog::installQtMessageHandler();
    ZLog::installTerminateHandler();

    // 创建主窗口
    QMainWindow w;
    w.setWindowTitle("ZLog 完整演示");
    QWidget central;
    QPushButton btnInfo("记录 INFO"), btnWarn("静默 WARNING"),
                btnCrit("阻塞弹窗 CRITICAL"), btnWin("模拟 Win 错误"),
                btnViewer("显示查看器");
    QVBoxLayout layout(&central);
    layout.addWidget(&btnInfo); layout.addWidget(&btnWarn);
    layout.addWidget(&btnCrit); layout.addWidget(&btnWin);
    layout.addWidget(&btnViewer);
    w.setCentralWidget(&central);
    w.show();

    // 设置弹窗父窗口
    ZLog::setDefaultPopupParent(&w);

    // 显示查看器
    ZLog::showLogViewer();

    // 连接信号
    QObject::connect(&btnInfo, &QPushButton::clicked, []() {
        LOG_INFO() << "INFO 按钮被点击";
    });
    QObject::connect(&btnWarn, &QPushButton::clicked, []() {
        LOG_WARNING().nopopup() << "磁盘使用率超过 90%，但已自动清理";
    });
    QObject::connect(&btnCrit, &QPushButton::clicked, []() {
        LOG_CRITICAL().popup(true).blocking(true) << "支付网关返回致命错误！";
        LOG_INFO() << "用户已确认致命错误弹窗";   // 弹窗关闭后才会执行
    });
    QObject::connect(&btnWin, &QPushButton::clicked, []() {
        DeleteFile(L"c:\\nonexistent_file.txt");
        LOG_WIN_ERROR_LAST("尝试删除不存在的文件");
    });
    QObject::connect(&btnViewer, &QPushButton::clicked, []() {
        ZLog::showLogViewer();
    });

    LOG_INFO() << "程序启动完成";

    return app.exec();
}
```

###block_green_start
运行该示例后：
- `logs/demo.log` 以 JSON 行格式持续记录所有日志。
- 控制台输出带 `[ID:n]` 前缀的彩色日志（需在 Qt Creator 中运行）。
- 查看器实时刷新，不同级别以不同颜色高亮。
- 点击“静默 WARNING”按钮不会弹出任何对话框。
- 点击“阻塞弹窗 CRITICAL”按钮会弹出模态错误框，程序暂停直到用户点击确认。
- “模拟 Win 错误”按钮会自动将系统错误码翻译为中文描述并记录。
###block_green_end

---

*ZLog 旨在让 Qt 桌面应用的日志记录像 `qDebug()` 一样简单，同时提供企业级的功能与友好的可视化界面。*
