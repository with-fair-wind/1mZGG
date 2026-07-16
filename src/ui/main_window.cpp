#include "dss/ui/widget/main_window.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFrame>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>

#include "dss/ui/support/app_event.h"

#ifdef DSS_HAS_ELA
#include <ElaMessageBar.h>
#include <ElaWindow.h>
#endif
namespace Dss::Ui {

namespace {

constexpr auto kWorkspaceSettingsGroup = "ui/main_window";
constexpr auto kGeometrySettingsKey = "geometry";
constexpr auto kDockStateSettingsKey = "dock_state";
constexpr int kDockStateVersion = 1;
constexpr auto kDockTabConfiguredProperty = "dss_dock_tab_configured";
constexpr auto kHiddenDockTitleBarObjectName = "hidden_tabified_dock_title_bar";
constexpr auto kFloatingGeometryInitializedProperty = "dss_floating_geometry_initialized";

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
    setupViewMenu();
    if (!restoreWorkspaceState()) {
        applyDefaultDockLayout();
    }
    connectSignals();
}
#else
MainWindow::MainWindow(MainViewModel& mainViewModel, QWidget* parent)
    : QMainWindow(parent), m_mainViewModel(mainViewModel) {
    setWindowTitle("DSS_QT v2.0 - Astronomical Image Processing");
    resize(1600, 1000);
    setupNavigation();
    setupViewMenu();
    if (!restoreWorkspaceState()) {
        applyDefaultDockLayout();
    }
    connectSignals();
}
#endif

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    saveWorkspaceState();
#ifdef DSS_HAS_ELA
    ElaWindow::closeEvent(event);
#else
    QMainWindow::closeEvent(event);
#endif
}

/// 创建各功能页并注册到原生 Dock 工作区。
void MainWindow::setupNavigation() {
    setupControlPage();
    setupDisplayPage();
    setupAnalysisPage();
    setupCommStatusPage();
    setupSettingsPage();
    setupLogPage();

    m_commPage = makeScrollablePage(m_commPage);
    setupDockWorkspace();
}

auto MainWindow::createPageDock(const QString& title, const QString& objectName, QWidget* page)
    -> QDockWidget* {
    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                      QDockWidget::DockWidgetClosable);
    dock->setWidget(page);
    return dock;
}

void MainWindow::setupDockWorkspace() {
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    m_pageDocks = {
        createPageDock("Control", "control_dock", m_controlPage),
        createPageDock("Display", "display_dock", m_displayPage),
        createPageDock("Analysis", "analysis_dock", m_analysisPage),
        createPageDock("Communication", "communication_dock", m_commPage),
        createPageDock("Settings", "settings_dock", m_settingsPage),
        createPageDock("Logs", "logs_dock", m_logPage),
    };

    for (auto* dock : m_pageDocks) {
        const auto refreshPresentation = [this] {
            QTimer::singleShot(0, this, [this] {
                setupDockTabInteractions();
                syncDockTitleBars();
            });
        };
        connect(dock, &QDockWidget::topLevelChanged, this,
                [dock, refreshPresentation](bool floating) {
                    if (floating) {
                        dock->setProperty(kFloatingGeometryInitializedProperty, true);
                    }
                    refreshPresentation();
                });
        connect(dock, &QDockWidget::dockLocationChanged, this,
                [refreshPresentation](Qt::DockWidgetArea) { refreshPresentation(); });
    }
}

void MainWindow::setupDockTabInteractions() {
    const auto tabBars = findChildren<QTabBar*>();
    for (auto* tabBar : tabBars) {
        bool containsPageDock = false;
        for (int index = 0; index < tabBar->count(); ++index) {
            if (dockForTab(*tabBar, index) != nullptr) {
                containsPageDock = true;
                break;
            }
        }
        if (!containsPageDock || tabBar->property(kDockTabConfiguredProperty).toBool()) {
            continue;
        }

        tabBar->setProperty(kDockTabConfiguredProperty, true);
        tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabBar, &QTabBar::tabBarDoubleClicked, this, [this, tabBar](int index) {
            auto* dock = dockForTab(*tabBar, index);
            if (dock == nullptr) {
                return;
            }
            floatDockFromTab(*dock);
        });
        connect(
            tabBar, &QWidget::customContextMenuRequested, this,
            [this, tabBar](const QPoint& position) { showDockTabContextMenu(*tabBar, position); });
    }
}

void MainWindow::syncDockTitleBars() {
    for (auto* dock : m_pageDocks) {
        const bool isTabified = !tabifiedDockWidgets(dock).isEmpty();
        const bool shouldHideTitleBar = !dock->isFloating() && isTabified;
        auto* titleBar = dock->titleBarWidget();
        const bool hasHiddenTitleBar =
            titleBar != nullptr && titleBar->objectName() == kHiddenDockTitleBarObjectName;

        if (shouldHideTitleBar && titleBar == nullptr) {
            auto* hiddenTitleBar = new QWidget(dock);
            hiddenTitleBar->setObjectName(kHiddenDockTitleBarObjectName);
            hiddenTitleBar->setFixedHeight(0);
            hiddenTitleBar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            dock->setTitleBarWidget(hiddenTitleBar);
        } else if (!shouldHideTitleBar && hasHiddenTitleBar) {
            dock->setTitleBarWidget(nullptr);
            titleBar->deleteLater();
        }
    }
}

auto MainWindow::dockForTab(const QTabBar& tabBar, int index) const -> QDockWidget* {
    if (index < 0 || index >= tabBar.count()) {
        return nullptr;
    }
    // Qt 未公开 tab→dock 映射,此处用 tabText(= Dock windowTitle)反向匹配。
    // 契约:各功能页 windowTitle 必须唯一且构造后不变(新增页面须遵守)。
    const auto title = tabBar.tabText(index);
    const auto dock = std::ranges::find_if(m_pageDocks, [&title](const QDockWidget* candidate) {
        return candidate != nullptr && candidate->windowTitle() == title;
    });
    return dock == m_pageDocks.end() ? nullptr : *dock;
}

void MainWindow::floatDockFromTab(QDockWidget& dock) {
    const bool needsInitialGeometry = !dock.property(kFloatingGeometryInitializedProperty).toBool();

    auto* titleBar = dock.titleBarWidget();
    if (titleBar != nullptr && titleBar->objectName() == kHiddenDockTitleBarObjectName) {
        dock.setTitleBarWidget(nullptr);
        titleBar->deleteLater();
    }

    dock.setFloating(true);
    dock.show();

    if (needsInitialGeometry) {
        const auto availableGeometry =
            screen() != nullptr ? screen()->availableGeometry() : frameGeometry();
        const QSize mainWindowRatioSize{width() * 2 / 3, height() * 2 / 3};
        const QSize contentHint = dock.widget() != nullptr ? dock.widget()->sizeHint() : QSize{};
        const QSize maximumSize{availableGeometry.width() * 4 / 5,
                                availableGeometry.height() * 4 / 5};
        const auto floatingSize =
            contentHint.expandedTo(mainWindowRatioSize).boundedTo(maximumSize);
        dock.resize(floatingSize);

        const auto frameSize = dock.frameSize();
        const auto mainCenter = frameGeometry().center();
        QPoint topLeft{mainCenter.x() - frameSize.width() / 2,
                       mainCenter.y() - frameSize.height() / 2};
        const int maximumX = availableGeometry.right() - frameSize.width() + 1;
        const int maximumY = availableGeometry.bottom() - frameSize.height() + 1;
        topLeft.setX(std::clamp(topLeft.x(), availableGeometry.left(),
                                std::max(availableGeometry.left(), maximumX)));
        topLeft.setY(std::clamp(topLeft.y(), availableGeometry.top(),
                                std::max(availableGeometry.top(), maximumY)));
        dock.move(topLeft);
        dock.setProperty(kFloatingGeometryInitializedProperty, true);
    }

    dock.raise();
    syncDockTitleBars();
}

void MainWindow::showDockTabContextMenu(QTabBar& tabBar, const QPoint& position) {
    auto* dock = dockForTab(tabBar, tabBar.tabAt(position));
    if (dock == nullptr) {
        return;
    }

    QMenu menu(&tabBar);
    auto* floatAction = menu.addAction(QString("Float %1").arg(dock->windowTitle()));
    auto* closeAction = menu.addAction(QString("Close %1").arg(dock->windowTitle()));
    const auto* selectedAction = menu.exec(tabBar.mapToGlobal(position));
    if (selectedAction == floatAction) {
        floatDockFromTab(*dock);
    } else if (selectedAction == closeAction) {
        dock->close();
    }
}

void MainWindow::applyDefaultDockLayout() {
    for (auto* dock : m_pageDocks) {
        dock->setFloating(false);
        removeDockWidget(dock);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        dock->show();
    }
    for (std::size_t index = 1; index < m_pageDocks.size(); ++index) {
        tabifyDockWidget(m_pageDocks.front(), m_pageDocks[index]);
    }
    m_pageDocks[1]->raise();
    setupDockTabInteractions();
    syncDockTitleBars();
}

void MainWindow::setupViewMenu() {
    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->setObjectName("view_menu");
    for (auto* dock : m_pageDocks) {
        viewMenu->addAction(dock->toggleViewAction());
    }
    viewMenu->addSeparator();
    auto* resetAction = viewMenu->addAction("Reset Default Layout");
    resetAction->setObjectName("reset_workspace_layout");
    connect(resetAction, &QAction::triggered, this, &MainWindow::resetWorkspaceLayout);
}

void MainWindow::saveWorkspaceState() const {
    QSettings settings;
    settings.beginGroup(kWorkspaceSettingsGroup);
    settings.setValue(kGeometrySettingsKey, saveGeometry());
    settings.setValue(kDockStateSettingsKey, saveState(kDockStateVersion));
    settings.endGroup();
}

auto MainWindow::restoreWorkspaceState() -> bool {
    QSettings settings;
    settings.beginGroup(kWorkspaceSettingsGroup);
    const auto geometry = settings.value(kGeometrySettingsKey).toByteArray();
    const auto dockState = settings.value(kDockStateSettingsKey).toByteArray();
    settings.endGroup();

    if (dockState.isEmpty()) {
        return false;
    }
    // 先恢复 Dock 状态;失败则几何也保持默认,避免"旧几何 + 默认布局"不一致
    const bool restored = restoreState(dockState, kDockStateVersion);
    if (!restored) {
        return false;
    }
    if (!geometry.isEmpty()) {
        (void)restoreGeometry(geometry);
    }
    setupDockTabInteractions();
    syncDockTitleBars();
    return true;
}

void MainWindow::resetWorkspaceLayout() {
    QSettings settings;
    settings.remove(kWorkspaceSettingsGroup);
    applyDefaultDockLayout();
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

    connect(&m_mainViewModel, &MainViewModel::statusTextChanged, this, [this](const QString& text) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::success(ElaMessageBarType::TopRight, "Status", text, 2000, this);
#else
        statusBar()->showMessage(text, 3000);
#endif
    });

    connect(tracking, &TrackingViewModel::trackInfoUpdated, this, [this](const QString& info) {
#ifdef DSS_HAS_ELA
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "Track", info, 3000, this);
#else
        statusBar()->showMessage(info, 5000);
#endif
    });
}

}  // namespace Dss::Ui
