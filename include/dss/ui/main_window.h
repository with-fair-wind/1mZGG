#pragma once

#include "dss/ui/view_model.h"

#ifdef DSS_HAS_ELA
#include <ElaWindow.h>
#else
#include <QMainWindow>
#endif

#include <memory>

namespace Dss::Ui {

class ImageDisplay;
class ImageDisplayCrop;

#ifdef DSS_HAS_ELA
class MainWindow : public ElaWindow
#else
class MainWindow : public QMainWindow
#endif
{
    Q_OBJECT

public:
    explicit MainWindow(ViewModel& vm, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupNavigation();
    void setupControlPage();
    void setupDisplayPage();
    void setupAnalysisPage();
    void setupCommStatusPage();
    void setupSettingsPage();
    void setupLogPage();

    void connectSignals();

    ViewModel& m_vm;

    QWidget* m_controlPage = nullptr;
    QWidget* m_displayPage = nullptr;
    QWidget* m_analysisPage = nullptr;
    QWidget* m_commPage = nullptr;
    QWidget* m_settingsPage = nullptr;
    QWidget* m_logPage = nullptr;

    ImageDisplay* m_imageDisplay = nullptr;
    ImageDisplayCrop* m_imageDisplayCrop = nullptr;
};

}  // namespace Dss::Ui
