#pragma once

#include "dss/ui/main_view_model.h"

#ifdef DSS_HAS_ELA
#include <ElaWindow.h>
#else
#include <QMainWindow>
#endif

#include <memory>

namespace Dss::Ui {

class ImageDisplay;
class GpuImageDisplay;

/// 应用主窗口，组织导航页面与图像显示控件
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
    ~MainWindow() override;

private:
    /// 初始化导航结构与各功能页
    void setupNavigation();
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

    QWidget* m_imageDisplayWidget = nullptr;  ///< 主图像显示控件容器
    ImageDisplay* m_imageDisplay = nullptr;   ///< CPU 主图像显示控件
#ifdef DSS_HAS_OPENGL_WIDGETS
    GpuImageDisplay* m_gpuImageDisplay = nullptr;  ///< GPU 主图像显示控件
#endif
};

}  // namespace Dss::Ui
