#include <QApplication>
#include <QDebug>

#include "dss/app/application_context.h"
#include "dss/core/config.h"
#include "dss/core/events.h"
#include "dss/ui/init_dialog.h"
#include "dss/ui/main_view_model.h"
#include "dss/ui/main_window.h"

#ifdef DSS_HAS_ELA
#include <ElaApplication.h>
#endif

/**
 * @brief 程序入口
 * 初始化 Qt 应用、加载配置、注册通信服务，显示初始化对话框后主窗口进入事件循环
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用退出码
 */
int main(int argc, char* argv[]) {
#ifdef DSS_HAS_ELA
    ElaApplication::getInstance()->init();
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("DSS_QT");
    QApplication::setApplicationVersion("2.0.0");
    QApplication::setOrganizationName("DPS");

    // 创建应用上下文并配置日志
    Dss::App::ApplicationContext context;
    context.wireLogger();

    // 加载系统配置文件
    auto configPath =
        QApplication::applicationDirPath().toStdString() + "/../config/SystemInit.json";
    if (auto result = context.loadConfig(configPath); !result) {
        qWarning() << "Config load warning:" << QString::fromStdString(result.error());
    }

    // 显示初始化对话框并逐步报告模块状态
    Dss::Ui::InitDialog initDialog;
    initDialog.show();
    initDialog.setStatus("Config", true);
    initDialog.setProgress(10);

    context.registerCommunicationServices();
    initDialog.setStatus("Communication", true);
    initDialog.setProgress(35);

    initDialog.setStatus("EventBus", true);
    initDialog.setProgress(100);

    // 创建 UI 层主 ViewModel 与主窗口，关闭初始化对话框后进入主界面
    Dss::Ui::MainViewModel mainViewModel(context.bus(), context.registry());
    Dss::Ui::MainWindow mainWindow(mainViewModel);
    mainWindow.show();

    initDialog.close();

    return QApplication::exec();
}
