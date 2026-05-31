#include "dss/app/application_context.h"
#include "dss/core/config.h"
#include "dss/core/events.h"
#include "dss/ui/init_dialog.h"
#include "dss/ui/main_window.h"
#include "dss/ui/view_model.h"

#include <QApplication>
#include <QDebug>

#ifdef DSS_HAS_ELA
#include <ElaApplication.h>
#endif

int main(int argc, char* argv[])
{
#ifdef DSS_HAS_ELA
    ElaApplication::getInstance()->init();
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("DSS_QT");
    QApplication::setApplicationVersion("2.0.0");
    QApplication::setOrganizationName("DPS");

    Dss::App::ApplicationContext context;
    context.wireLogger();

    auto configPath = QApplication::applicationDirPath().toStdString() + "/../config/SystemInit.ini";
    if (auto result = context.loadConfig(configPath); !result)
    {
        qWarning() << "Config load warning:" << QString::fromStdString(result.error());
    }

    Dss::Ui::InitDialog initDialog;
    initDialog.show();
    initDialog.setStatus("Config", true);
    initDialog.setProgress(10);

    // TODO: Phase 3 - Initialize serial channels and register with ServiceRegistry
    // TODO: Phase 3 - Initialize network channels
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
