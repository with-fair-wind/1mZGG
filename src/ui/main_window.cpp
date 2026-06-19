#include "dss/ui/main_window.h"

#include <QFrame>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>

#include "dss/ui/app_event.h"

#ifdef DSS_HAS_ELA
#include <ElaMessageBar.h>
#include <ElaWindow.h>
#endif
namespace Dss::Ui {

namespace {

/// @brief 将日志快照按级别颜色渲染到文本控件。
/// @brief 为内容较高的页面创建可滚动容器，避免页面最小高度撑大主窗口。
/// @param content 页面内容。
/// @return 可直接注册到主导航的滚动区域。
auto makeScrollablePage(QWidget* content) -> QScrollArea* {
    auto* scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(content);
    return scrollArea;
}

}  // namespace

#ifdef DSS_HAS_ELA
MainWindow::MainWindow(MainViewModel& mainViewModel, QWidget* parent)
    : ElaWindow(parent), m_mainViewModel(mainViewModel) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    connectSignals();
}
#else
MainWindow::MainWindow(MainViewModel& mainViewModel, QWidget* parent)
    : QMainWindow(parent), m_mainViewModel(mainViewModel) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    connectSignals();
}
#endif

MainWindow::~MainWindow() = default;

/// 创建各功能页并注册到导航（Ela 侧栏或 QTabWidget）
void MainWindow::setupNavigation() {
    setupControlPage();
    setupDisplayPage();
    setupAnalysisPage();
    setupCommStatusPage();
    setupSettingsPage();
    setupLogPage();

#ifdef DSS_HAS_ELA
    addPageNode("Control", m_controlPage, ElaIconType::Gamepad);
    addPageNode("Display", m_displayPage, ElaIconType::Display);
    addPageNode("Analysis", m_analysisPage, ElaIconType::ChartLine);
    addPageNode("Communication", makeScrollablePage(m_commPage), ElaIconType::Satellite);
    addPageNode("Settings", m_settingsPage, ElaIconType::Gear);
    addPageNode("Logs", m_logPage, ElaIconType::FileLines);
#else
    auto* tabs = new QTabWidget(this);
    tabs->addTab(m_controlPage, "Control");
    tabs->addTab(m_displayPage, "Display");
    tabs->addTab(m_analysisPage, "Analysis");
    tabs->addTab(makeScrollablePage(m_commPage), "Communication");
    tabs->addTab(m_settingsPage, "Settings");
    tabs->addTab(m_logPage, "Logs");
    setCentralWidget(tabs);
#endif
}

/// 构建序列选择、回放控制、处理/跟踪模式及统计信息显示
/// 放置主图像显示控件，并绑定显示子 ViewModel 与 AppEvent
/// @brief 创建日志页并连接日志子 ViewModel 事件。
/// 连接 AppEvent 与 UI 层主 ViewModel、子 ViewModel，并在状态栏/MessageBar 显示反馈
void MainWindow::connectSignals() {
    auto* display = &m_mainViewModel.display();
    auto* tracking = &m_mainViewModel.tracking();

    connect(&AppEvent::instance(), &AppEvent::targetPositionSelected, tracking,
            &TrackingViewModel::selectTarget);
    connect(&AppEvent::instance(), &AppEvent::zoomLevelChanged, display,
            &DisplayViewModel::toggleZoom);

    connect(&m_mainViewModel, &MainViewModel::statusTextChanged, [this](const QString& text) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::success(ElaMessageBarType::TopRight, "Status", text, 2000, this);
#else
        statusBar()->showMessage(text, 3000);
#endif
    });

    connect(tracking, &TrackingViewModel::trackInfoUpdated, [this](const QString& info) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "Track", info, 3000, this);
#else
        statusBar()->showMessage(info, 5000);
#endif
    });
}

}  // namespace Dss::Ui
