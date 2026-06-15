#pragma once

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/core/types.h"

namespace Dss::Ui {

/// @brief UI 层可编辑的 UDP 网络端点状态。
struct NetworkEndpointState {
    QString key;           ///< 端点键名，用于回写配置
    QString displayName;   ///< 界面显示名称
    QString localIp;       ///< 本地 IP
    int localPort = 0;     ///< 本地端口，0 表示自动分配
    QString remoteIp;      ///< 远端 IP
    int remotePort = 0;    ///< 远端端口
    bool canOpen = false;  ///< 是否支持 UI 显式打开/关闭
    bool isOpen = false;   ///< 对应服务当前是否已打开
};

/// @brief UI 层展示和控制串口通道所需的状态快照。
struct SerialChannelState {
    QString key;                    ///< 串口通道键名，用于打开/关闭服务
    QString displayName;            ///< 界面显示名称
    QString portName;               ///< 串口端口名称
    int baudRate = 0;               ///< 波特率
    int dataBits = 0;               ///< 数据位
    int stopBits = 0;               ///< 停止位
    std::size_t recvFrameSize = 0;  ///< 接收帧固定字节长度
    std::size_t sendFrameSize = 0;  ///< 发送帧固定字节长度
    bool isRegistered = false;      ///< 服务注册表中是否存在该串口服务
    bool isOpen = false;            ///< 串口服务当前是否已打开
};

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
    Q_PROPERTY(QString dataExchangeGxtcLocalIp READ dataExchangeGxtcLocalIp NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(int dataExchangeGxtcLocalPort READ dataExchangeGxtcLocalPort NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(QString dataExchangeGxtcRemoteIp READ dataExchangeGxtcRemoteIp NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(int dataExchangeGxtcRemotePort READ dataExchangeGxtcRemotePort NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(QString dataExchangeGdclLocalIp READ dataExchangeGdclLocalIp NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(int dataExchangeGdclLocalPort READ dataExchangeGdclLocalPort NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(QString dataExchangeGdclRemoteIp READ dataExchangeGdclRemoteIp NOTIFY
                   dataExchangeEndpointsChanged)
    Q_PROPERTY(int dataExchangeGdclRemotePort READ dataExchangeGdclRemotePort NOTIFY
                   dataExchangeEndpointsChanged)
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
    /// @brief 获取当前所有可编辑 UDP 网络端点。
    [[nodiscard]] auto networkEndpointConfigs() -> std::vector<NetworkEndpointState>;
    /// @brief 查询指定网络端点对应服务是否已打开。
    [[nodiscard]] bool isNetworkEndpointOpen(const QString& key);
    /// @brief 获取当前所有串口通道配置与运行状态。
    [[nodiscard]] auto serialChannelConfigs() -> std::vector<SerialChannelState>;
    /// @brief 查询指定串口通道服务是否已打开。
    [[nodiscard]] bool isSerialChannelOpen(const QString& key);
    /// GXTC 数据交换本地 IP
    [[nodiscard]] QString dataExchangeGxtcLocalIp() const;
    /// GXTC 数据交换本地端口
    [[nodiscard]] int dataExchangeGxtcLocalPort() const;
    /// GXTC 数据交换远端 IP
    [[nodiscard]] QString dataExchangeGxtcRemoteIp() const;
    /// GXTC 数据交换远端端口
    [[nodiscard]] int dataExchangeGxtcRemotePort() const;
    /// GDCL 数据交换本地 IP
    [[nodiscard]] QString dataExchangeGdclLocalIp() const;
    /// GDCL 数据交换本地端口
    [[nodiscard]] int dataExchangeGdclLocalPort() const;
    /// GDCL 数据交换远端 IP
    [[nodiscard]] QString dataExchangeGdclRemoteIp() const;
    /// GDCL 数据交换远端端口
    [[nodiscard]] int dataExchangeGdclRemotePort() const;
    /// 状态栏文本
    [[nodiscard]] QString statusText() const {
        return m_statusText;
    }
    /// 日志页最小显示级别
    [[nodiscard]] int logMinimumLevel() const {
        return static_cast<int>(m_logMinimumLevel);
    }
    /// 获取当前过滤后的日志文本快照
    [[nodiscard]] QStringList visibleLogEntries() const;
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
    /// @brief 应用单个 UDP 网络端点配置
    /// @return 参数合法并写入内存配置时返回 true
    Q_INVOKABLE bool applyNetworkEndpointConfig(const QString& key, const QString& localIp,
                                                int localPort, const QString& remoteIp,
                                                int remotePort);
    /// @brief 显式打开指定网络端点对应的服务
    /// @return 成功打开或服务已打开时返回 true
    Q_INVOKABLE bool openNetworkEndpoint(const QString& key);
    /// @brief 显式关闭指定网络端点对应的服务
    Q_INVOKABLE void closeNetworkEndpoint(const QString& key);
    /// @brief 显式打开指定串口通道服务
    /// @return 成功打开或服务已打开时返回 true
    Q_INVOKABLE bool openSerialChannel(const QString& key);
    /// @brief 显式关闭指定串口通道服务
    Q_INVOKABLE void closeSerialChannel(const QString& key);
    /// @brief 应用单个串口通道配置
    /// @return 参数合法并写入内存配置时返回 true
    Q_INVOKABLE bool applySerialChannelConfig(const QString& key, const QString& portName,
                                              int baudRate, int dataBits, int stopBits);
    /// @brief 发送曝光通道联调命令
    /// @return 参数合法、命令服务存在且串口已打开时返回 true
    Q_INVOKABLE bool sendExposureCommand(bool freeRun, int frameFrequencyCode,
                                         int exposureDelayTicks);
    /// @brief 发送伺服距离/速度修正联调命令
    /// @return 参数合法、命令服务存在且串口已打开时返回 true
    Q_INVOKABLE bool sendServoCorrection(bool distanceValid, bool speedValid,
                                         double distanceXArcsec, double distanceYArcsec,
                                         double speedXArcsecPerSec, double speedYArcsecPerSec,
                                         int mode);
    /// @brief 发送主控状态回包联调命令
    /// @return 参数合法、命令服务存在且串口已打开时返回 true
    Q_INVOKABLE bool sendMasterControlStatus(int year, int month, int day, int hour, int minute,
                                             int second, int millisecond, double azimuthDegrees,
                                             double elevationDegrees, bool distanceValid,
                                             bool speedValid, double distanceXArcsec,
                                             double distanceYArcsec, double speedXArcsecPerSec,
                                             double speedYArcsecPerSec, int servoMode);
    /// @brief 应用 GXTC/GDCL 数据交换端点配置
    /// @return 参数合法并写入内存配置时返回 true
    Q_INVOKABLE bool applyDataExchangeEndpoints(const QString& gxtcLocalIp, int gxtcLocalPort,
                                                const QString& gxtcRemoteIp, int gxtcRemotePort,
                                                const QString& gdclLocalIp, int gdclLocalPort,
                                                const QString& gdclRemoteIp, int gdclRemotePort);
    /// 显式打开 GXTC/GDCL 数据交换 UDP 通道
    /// 成功返回 true，失败返回 false
    Q_INVOKABLE bool openDataExchange();
    /// 显式关闭 GXTC/GDCL 数据交换 UDP 通道
    Q_INVOKABLE void closeDataExchange();
    /// 设置日志页最小显示级别
    Q_INVOKABLE void setLogMinimumLevel(int level);
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
    /// 所有网络端点配置变化
    void networkEndpointsChanged();
    /// 网络服务打开状态变化
    void networkServiceStateChanged(const QString& key, bool isOpen);
    /// 所有串口通道状态变化
    void serialChannelsChanged();
    /// 单个串口通道打开状态变化
    void serialChannelStateChanged(const QString& key, bool isOpen);
    /// 数据交换端点配置变化
    void dataExchangeEndpointsChanged();
    /// 状态文本变化
    void statusTextChanged(const QString& text);
    /// @brief 新日志文本追加到 UI 日志页。
    /// @param text 单条日志文本
    void logEntryAppended(const QString& text);
    /// @brief 日志页可见文本快照变化。
    /// @param entries 当前过滤条件下的日志文本列表
    void logEntriesChanged(const QStringList& entries);
    /// @brief 日志页最小显示级别变化。
    /// @param level 最小显示级别，含义与 Dss::Core::LogLevel 数值一致
    void logMinimumLevelChanged(int level);
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
    /// UI 日志缓存项，保留日志级别和最终展示文本。
    struct UiLogEntry {
        Dss::Core::LogLevel level = Dss::Core::LogLevel::Info;  ///< 日志级别
        QString text;                                           ///< 展示文本
    };

    /// UI 日志缓存最大条数
    static constexpr std::size_t kMaxLogEntries = 500;

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
    /// 处理串口接收帧校验失败事件
    void onSerialFrameError(const Dss::Core::SerialFrameErrorEvent& event);
    /// 处理串口协议字段解码失败事件
    void onSerialDecodeError(const Dss::Core::SerialDecodeErrorEvent& event);
    /// 处理核心日志事件
    void onLogMessage(const Dss::Core::LogMessageEvent& event);
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
    /// 追加一条 UI 日志并维护缓存容量
    void appendLogEntry(Dss::Core::LogLevel level, QString text);
    /// 判断日志级别是否满足当前 UI 过滤条件
    [[nodiscard]] bool isLogLevelVisible(Dss::Core::LogLevel level) const;

    MessageBus& m_bus;                       ///< 事件总线引用
    Dss::Core::ServiceRegistry& m_registry;  ///< 服务注册表引用

    bool m_grabbing = false;   ///< 是否正在采集/回放
    double m_frameRate = 0.0;  ///< 当前帧率
    int m_processingMode = static_cast<int>(Dss::Core::ProcessingMode::None);  ///< 处理模式
    int m_trackMode = static_cast<int>(Dss::Core::TrackMode::Init);            ///< 跟踪模式
    double m_exposure = 0.0;                                                   ///< 曝光时间（毫秒）
    bool m_saving = false;                                                     ///< 是否正在保存
    bool m_dataExchangeOpen = false;  ///< 数据交换 UDP 是否已显式打开
    QString m_statusText = "Ready";   ///< 状态栏文本
    Dss::Core::LogLevel m_logMinimumLevel = Dss::Core::LogLevel::Info;  ///< 日志页最小显示级别
    int m_replayFrameCount = 0;                                         ///< 回放序列总帧数
    int m_replayCurrentFrame = 0;                                       ///< 回放当前帧索引
    std::optional<Dss::Core::MeasuredBlob> m_manualTarget;              ///< 手动选定的跟踪目标
    std::vector<UiLogEntry> m_logEntries;  ///< UI 日志缓存，最多保留最近 500 条

    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表
};

}  // namespace Dss::Ui
