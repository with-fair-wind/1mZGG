#include <QApplication>
#include <QDebug>

#include "dss/app/application_context.h"
#include "dss/core/config.h"
#include "dss/core/events.h"
#include "dss/ui/init_dialog.h"
#include "dss/ui/main_window.h"
#include "dss/ui/view_model.h"

#ifdef DSS_HAS_ELA
#include <ElaApplication.h>
#endif

int main(int argc, char* argv[]) {
#ifdef DSS_HAS_ELA
    ElaApplication::getInstance()->init();
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("DSS_QT");
    QApplication::setApplicationVersion("2.0.0");
    QApplication::setOrganizationName("DPS");

    Dss::App::ApplicationContext context;
    context.wireLogger();

    auto configPath =
        QApplication::applicationDirPath().toStdString() + "/../config/SystemInit.json";
    if (auto result = context.loadConfig(configPath); !result) {
        qWarning() << "Config load warning:" << QString::fromStdString(result.error());
    }

    Dss::Ui::InitDialog initDialog;
    initDialog.show();
    initDialog.setStatus("Config", true);
    initDialog.setProgress(10);

    context.registerCommunicationServices();
    initDialog.setStatus("Communication", true);
    initDialog.setProgress(35);

    // TODO: Phase 4 - Create ImageProcessor and register strategies
    // TODO: Phase 5 - Initialize CudaDeviceManager

    initDialog.setStatus("EventBus", true);
    initDialog.setProgress(100);

    Dss::Ui::ViewModel viewModel(context.bus(), context.registry());
    Dss::Ui::MainWindow mainWindow(viewModel);
    mainWindow.show();

    initDialog.close();

    return QApplication::exec();
}
