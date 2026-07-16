#pragma once

#include "dss/ui/view_model/main_view_model.h"

#ifdef DSS_HAS_ELA
#include <ElaWindow.h>
#else
#include <QMainWindow>
#endif

#include <array>
#include <memory>

class QDockWidget;
class QCloseEvent;
class QPoint;
class QString;
class QTabBar;

namespace Dss::Ui {

class ImageDisplay;
class GpuImageDisplay;

/// 应用主窗口，组织可停靠、浮动和分组显示的功能页面。
#ifdef DSS_HAS_ELA
class MainWindow : public ElaWindow
#else
class MainWindow : public QMainWindow
#endif
{
    Q_OBJECT

public:
    /**
     * @brief 构造主窗口
     * @param mainViewModel UI 层主 ViewModel 引用
     * @param parent Qt 父窗口
     */
    explicit MainWindow(MainViewModel& mainViewModel, QWidget* parent = nullptr);

    /** @brief 销毁主窗口及其 Dock 页面。 */
    ~MainWindow() override;

protected:
    /**
     * @brief 关闭主窗口前保存窗口几何信息与 Dock 布局。
     * @param event Qt 关闭事件。
     */
    void closeEvent(QCloseEvent* event) override;

private:
    /// 初始化 Dock 工作区与各功能页。
    void setupNavigation();
    /**
     * @brief 将业务页面包装为具有稳定身份的 Dock。
     * @param title Dock 标题。
     * @param objectName 用于布局持久化的稳定对象名。
     * @param page 由 Dock 接管所有权的业务页面。
     * @return 已创建并归属主窗口的 Dock。
     */
    auto createPageDock(const QString& title, const QString& objectName, QWidget* page)
        -> QDockWidget*;
    /// 构建所有功能页的 Dock 容器。
    void setupDockWorkspace();
    /// 为原生 Dock 标签栏注册双击浮动和右键菜单交互。
    void setupDockTabInteractions();
    /// 根据当前停靠关系同步 Dock 标题栏：标签化时隐藏，浮动或独立停靠时恢复。
    void syncDockTitleBars();
    /**
     * @brief 查找标签栏指定位置对应的功能页 Dock。
     * @param tabBar Qt 主窗口创建的原生 Dock 标签栏。
     * @param index 标签索引。
     * @return 匹配的 Dock；索引无效或并非功能页标签时返回 nullptr。
     */
    auto dockForTab(const QTabBar& tabBar, int index) const -> QDockWidget*;
    /**
     * @brief 将标签页 Dock 以合理的首次尺寸浮动到主窗口所在屏幕。
     * @param dock 需要浮动的功能页 Dock。
     */
    void floatDockFromTab(QDockWidget& dock);
    /**
     * @brief 显示功能页标签的浮动与关闭菜单。
     * @param tabBar 触发菜单的 Dock 标签栏。
     * @param position 标签栏局部坐标。
     */
    void showDockTabContextMenu(QTabBar& tabBar, const QPoint& position);
    /// 将全部 Dock 恢复为默认顶部 Tab 布局。
    void applyDefaultDockLayout();
    /// 创建标准 View 菜单及各 Dock 的显示开关。
    void setupViewMenu();
    /// 保存窗口几何信息与版本化 Dock 状态。
    void saveWorkspaceState() const;
    /// 恢复窗口几何信息与版本化 Dock 状态。
    /// @return 状态存在且完整恢复时返回 true。
    auto restoreWorkspaceState() -> bool;
    /// 清除已保存状态并恢复默认 Dock 布局。
    void resetWorkspaceLayout();
    /// 构建控制页（序列选择、采集、处理/跟踪模式等）
    void setupControlPage();
    /// 构建显示页（主图像显示）
    void setupDisplayPage();
    /// 构建分析页
    void setupAnalysisPage();
    /// 构建通信状态页
    void setupCommStatusPage();
    /// 构建设置页
    void setupSettingsPage();
    /// 构建日志页
    void setupLogPage();

    /// 连接 UI 层主 ViewModel、子 ViewModel 与 AppEvent 信号槽
    void connectSignals();

    MainViewModel& m_mainViewModel;  ///< UI 层主 ViewModel 引用

    QWidget* m_controlPage = nullptr;   ///< 控制页容器
    QWidget* m_displayPage = nullptr;   ///< 显示页容器
    QWidget* m_analysisPage = nullptr;  ///< 分析页容器
    QWidget* m_commPage = nullptr;      ///< 通信状态页容器
    QWidget* m_settingsPage = nullptr;  ///< 设置页容器
    QWidget* m_logPage = nullptr;       ///< 日志页容器

    std::array<QDockWidget*, 6> m_pageDocks{};  ///< 固定顺序的功能页 Dock。

    QWidget* m_imageDisplayWidget = nullptr;  ///< 主图像显示控件容器
    ImageDisplay* m_imageDisplay = nullptr;   ///< CPU 主图像显示控件
#ifdef DSS_HAS_OPENGL_WIDGETS
    GpuImageDisplay* m_gpuImageDisplay = nullptr;  ///< GPU 主图像显示控件
#endif
};

}  // namespace Dss::Ui
