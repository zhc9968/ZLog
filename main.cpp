#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>
#include <windows.h>
#include <comdef.h>

#include "zlog.h"
#include "zlogviewer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // ===== 1. 初始化 ZLog =====
    ZLog::instance()->init("ZLogDemo",            // 应用名
                           "logs/demo.json",       // 日志文件路径
                           ZLog::DEBUG,           // 最低记录级别
                           ZLog::SizeRolling,     // 文件滚动策略
                           1 * 1024 * 1024,       // 1 MB 滚动（方便测试）
                           3,                     // 保留3个备份
                           true);                 // 输出到 qDebug


    // ===== 2. 创建主窗口用于父窗口测试 =====
    QMainWindow mainWindow;
    mainWindow.setWindowTitle("ZLog 测试窗口");
    QWidget central;
    QVBoxLayout layout(&central);
    QPushButton btnInfo("记录 INFO（弹窗，阻塞）"), btnWarn("记录 WARNING（不弹窗）");
    QPushButton btnCrit("记录 CRITICAL（弹窗，非阻塞）"), btnWin("触发 WinAPI 错误");
    QPushButton btnCom("触发 COM 错误"), btnShowViewer("显示日志查看器");
    layout.addWidget(&btnInfo); layout.addWidget(&btnWarn);
    layout.addWidget(&btnCrit); layout.addWidget(&btnWin);
    layout.addWidget(&btnCom); layout.addWidget(&btnShowViewer);
    mainWindow.setCentralWidget(&central);
    mainWindow.show();

    // 设置全局弹窗父窗口（使弹窗居中在主窗口上）
    ZLog::setDefaultPopupParent(&mainWindow);

    // ===== 3. 显示日志查看器 =====
    //ZLog::showLogViewer();   // 默认启用，主动显示

    // ===== 4. 基本日志测试 =====
    LOG_INFO() << "程序启动，版本 2.0";
    LOG_DEBUG() << "调试信息：详细参数 x=" << 42;
    LOG_QUESTION().nopopup() << "这是一条不弹窗的询问日志";
    LOG_WARNING().blocking(false) << "这是一条非阻塞警告（弹窗会异步出现）";

    // ===== 5. 按钮槽函数：测试弹窗控制、阻塞、父窗口 =====
    QObject::connect(&btnInfo, &QPushButton::clicked, [&]() {
        // INFO 默认弹窗（遵循全局开关），阻塞，父窗口为 mainWindow
        LOG_INFO() << "用户点击了 INFO 按钮";
    });

    QObject::connect(&btnWarn, &QPushButton::clicked, [&]() {
        // 不弹窗，不阻塞
        LOG_WARNING().nopopup().blocking(false) << "磁盘空间低，已后台记录";
    });

    QObject::connect(&btnCrit, &QPushButton::clicked, [&]() {
        // 强制弹窗，非阻塞
        LOG_CRITICAL().popup(true).blocking(false) << "严重错误：网络连接丢失";
    });

    // ===== 6. WinAPI 错误测试 =====
    QObject::connect(&btnWin, &QPushButton::clicked, [&]() {
        // 故意删除不存在的文件，触发 Win32 错误
        DeleteFile(L"c:\\nonexistent_file_test.txt");
        LOG_WIN_ERROR_LAST("测试删除不存在的文件").blocking();
    });

    // ===== 7. COM 错误测试 =====
    QObject::connect(&btnCom, &QPushButton::clicked, [&]() {
        // 故意使用无效 CLSID 触发 COM 错误
        CLSID clsid;
        HRESULT hr = CLSIDFromProgID(L"NonExistent.Component", &clsid);
        if (FAILED(hr)) {
            LOG_COM_ERROR(hr, "测试加载不存在的 COM 组件");
        } else {
            LOG_INFO() << "COM 组件竟然存在？（意料之外）";
        }
    });

    // ===== 8. 按钮：显示日志查看器（若已关闭） =====
    QObject::connect(&btnShowViewer, &QPushButton::clicked, [&]() {
        ZLog::showLogViewer();
    });

    // ===== 9. 运行应用 =====
    return app.exec();
}