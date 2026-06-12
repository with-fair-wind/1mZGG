#pragma once

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <memory>
#include <optional>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/core/types.h"

namespace Dss::Ui {

/// 应用 ViewModel，桥接 UI 与后端服务（事件总线、服务注册表）
class ViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isGrabbing READ isGrabbing NOTIFY grabbingChanged)
    Q_PROPERTY(double frameRate READ frameRate NOTIFY frameRateChanged)
    Q_PROPERTY(
        int processingMode READ processingMode WRITE setProcessingMode NOTIFY processingModeChanged)
    Q_PROPERTY(int trackMode READ trackMode WRITE setTrackMode NOTIFY trackModeChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY exposureChanged)
    Q_PROPERTY(bool isSaving READ isSaving NOTIFY savingChanged)
    Q_PROPERTY(bool isDataExchangeOpen READ isDataExchangeOpen NOTIFY dataExchangeOpenChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(int replayFrameCount READ replayFrameCount NOTIFY replayFrameCountChanged)
    Q_PROPERTY(int replayCurrentFrame READ replayCurrentFrame NOTIFY replayCurrentFrameChanged)

public:
    using MessageBus = Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名

    /**
     * @brief 构造 ViewModel
     * @param bus 事件总线引用
     * @param registry 服务注册表引用
     * @param parent Qt 父对象
     */
    explicit ViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry,
                       QObject* parent = nullptr);
    ~ViewModel() override;

    /// 是否正在采集/回放
    [[nodiscard]] bool isGrabbing() const {
        return m_grabbing;
    }
    /// 当前帧率
    [[nodiscard]] double frameRate() const {
        return m_frameRate;
    }
    /// 图像处理模式
    [[nodiscard]] int processingMode() const {
        return m_processingMode;
    }
    /// 跟踪模式
    [[nodiscard]] int trackMode() const {
        return m_trackMode;
    }
    /// 曝光时间（毫秒）
    [[nodiscard]] double exposure() const {
        return m_exposure;
    }
    /// 是否正在保存数据
    [[nodiscard]] bool isSaving() const {
        return m_saving;
    }
    /// 数据交换 UDP 是否已显式打开
    [[nodiscard]] bool isDataExchangeOpen() const {
        return m_dataExchangeOpen;
    }
    /// 状态栏文本
    [[nodiscard]] QString statusText() const {
        return m_statusText;
    }
    /// 回放序列总帧数
    [[nodiscard]] int replayFrameCount() const {
        return m_replayFrameCount;
    }
    /// 回放当前帧索引
    [[nodiscard]] int replayCurrentFrame() const {
        return m_replayCurrentFrame;
    }

    /**
     * @brief 选择回放图像序列文件
     * @param files 图像文件路径列表
     * @return 成功返回 true，失败返回 false
     */
    Q_INVOKABLE bool selectReplayFiles(const QStringList& files);
    /// 开始采集/回放
    Q_INVOKABLE void startGrab();
    /// 停止采集/回放
    Q_INVOKABLE void stopGrab();
    /**
     * @brief 单步前进回放一帧
     * @return 成功返回 true，失败返回 false
     */
    Q_INVOKABLE bool stepReplayForward();
    /// 设置图像处理模式
    Q_INVOKABLE void setProcessingMode(int mode);
    /// 设置跟踪模式
    Q_INVOKABLE void setTrackMode(int mode);
    /// 设置曝光时间（毫秒）
    Q_INVOKABLE void setExposure(double ms);
    /// 在图像坐标中选择手动跟踪目标
    Q_INVOKABLE void selectTarget(QPointF pos);
    /// 开始保存图像与跟踪数据
    Q_INVOKABLE void startSaving();
    /// 停止保存
    Q_INVOKABLE void stopSaving();
    /// 显式打开 GXTC/GDCL 数据交换 UDP 通道
    /// 成功返回 true，失败返回 false
    Q_INVOKABLE bool openDataExchange();
    /// 显式关闭 GXTC/GDCL 数据交换 UDP 通道
    Q_INVOKABLE void closeDataExchange();
    /// 切换缩放级别
    Q_INVOKABLE void toggleZoom(int level);

signals:
    /// 采集/回放状态变化
    void grabbingChanged(bool value);
    /// 帧率变化
    void frameRateChanged(double value);
    /// 处理模式变化
    void processingModeChanged(int mode);
    /// 跟踪模式变化
    void trackModeChanged(int mode);
    /// 曝光时间变化
    void exposureChanged(double ms);
    /// 保存状态变化
    void savingChanged(bool value);
    /// 数据交换 UDP 打开状态变化
    void dataExchangeOpenChanged(bool value);
    /// 状态文本变化
    void statusTextChanged(const QString& text);
    /// 回放总帧数变化
    void replayFrameCountChanged(int count);
    /// 回放当前帧变化
    void replayCurrentFrameChanged(int frame);

    /// 主显示图像就绪
    void displayImageReady(QImage image);
    /// 裁剪区域图像就绪
    void cropImageReady(QImage image);
    /// 跟踪信息更新
    void trackInfoUpdated(const QString& info);
    /// 目标列表数量更新
    void targetListUpdated(int count);
    /// 图像统计量更新（最小值、最大值、均值、标准差）
    void imageStatsUpdated(double minVal, double maxVal, double avg, double stdDev);

private:
    /// 订阅后端事件
    void setupSubscriptions();

    /// 处理显示刷新事件
    void onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event);
    /// 处理图像处理完成事件
    void onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event);
    /// 处理跟踪结果事件
    void onTrackResult(const Dss::Core::TrackResultEvent& event);
    /// 处理主控指令事件
    void onMasterControl(const Dss::Core::MasterControlEvent& event);
    /// 处理网络发送失败事件
    void onNetworkTransmissionError(const Dss::Core::NetworkTransmissionErrorEvent& event);
    /// 根据当前模式配置处理策略
    void configureProcessingStrategy();
    /// 根据当前模式配置跟踪策略
    void configureTrackingStrategy();
    /// 由点击位置构造手动跟踪目标
    [[nodiscard]] static auto makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob;
    /// 设置回放当前帧并发出信号
    void setReplayCurrentFrame(int frame);
    /// 设置数据交换 UDP 打开状态并发出信号
    void setDataExchangeOpen(bool value);
    /// 更新状态文本并发出信号
    void setStatus(QString text);

    MessageBus& m_bus;                       ///< 事件总线引用
    Dss::Core::ServiceRegistry& m_registry;  ///< 服务注册表引用

    bool m_grabbing = false;   ///< 是否正在采集/回放
    double m_frameRate = 0.0;  ///< 当前帧率
    int m_processingMode = static_cast<int>(Dss::Core::ProcessingMode::None);  ///< 处理模式
    int m_trackMode = static_cast<int>(Dss::Core::TrackMode::Init);            ///< 跟踪模式
    double m_exposure = 0.0;                                                   ///< 曝光时间（毫秒）
    bool m_saving = false;                                                     ///< 是否正在保存
    bool m_dataExchangeOpen = false;                        ///< 数据交换 UDP 是否已显式打开
    QString m_statusText = "Ready";                         ///< 状态栏文本
    int m_replayFrameCount = 0;                             ///< 回放序列总帧数
    int m_replayCurrentFrame = 0;                           ///< 回放当前帧索引
    std::optional<Dss::Core::MeasuredBlob> m_manualTarget;  ///< 手动选定的跟踪目标

    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表
};

}  // namespace Dss::Ui
