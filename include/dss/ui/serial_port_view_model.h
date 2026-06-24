#pragma once

#include <QObject>
#include <QString>
#include <cstddef>
#include <vector>

#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief UI 层展示和控制串口通道所需的状态快照。
 */
struct SerialChannelState {
    QString key;                    ///< 串口通道键名，用于打开/关闭服务。
    QString displayName;            ///< 界面显示名称。
    QString portName;               ///< 串口端口名称。
    int baudRate = 0;               ///< 波特率。
    int dataBits = 0;               ///< 数据位。
    int stopBits = 0;               ///< 停止位。
    std::size_t recvFrameSize = 0;  ///< 接收帧固定字节长度。
    std::size_t sendFrameSize = 0;  ///< 发送帧固定字节长度。
    bool isRegistered = false;      ///< 服务注册表中是否存在该串口服务。
    bool isOpen = false;            ///< 串口服务当前是否已打开。
};

/**
 * @brief 串口子 ViewModel，负责四路串口配置、显式开关和联调命令。
 */
class SerialPortViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造串口子 ViewModel。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit SerialPortViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构串口子 ViewModel。
     */
    ~SerialPortViewModel() override;

    /**
     * @brief 获取当前所有串口通道配置与运行状态。
     * @return 串口状态快照列表。
     */
    [[nodiscard]] auto serialChannelConfigs() -> std::vector<SerialChannelState>;

    /**
     * @brief 查询指定串口通道服务是否已打开。
     * @param key 串口通道键名。
     * @return 服务已打开时返回 true。
     */
    [[nodiscard]] bool isSerialChannelOpen(const QString& key);

public Q_SLOTS:
    /**
     * @brief 显式打开指定串口通道服务。
     * @param key 串口通道键名。
     * @return 成功打开或服务已打开时返回 true。
     */
    Q_INVOKABLE bool openSerialChannel(const QString& key);

    /**
     * @brief 显式关闭指定串口通道服务。
     * @param key 串口通道键名。
     */
    Q_INVOKABLE void closeSerialChannel(const QString& key);

    /**
     * @brief 应用单个串口通道配置。
     * @param key 串口通道键名。
     * @param portName 串口设备名称。
     * @param baudRate 波特率。
     * @param dataBits 数据位数。
     * @param stopBits 停止位枚举值。
     * @return 参数合法并写入内存配置时返回 true。
     */
    Q_INVOKABLE bool applySerialChannelConfig(const QString& key, const QString& portName,
                                              int baudRate, int dataBits, int stopBits);

    /**
     * @brief 发送曝光通道联调命令。
     * @param freeRun 是否启用自由运行模式。
     * @param frameFrequencyCode 帧频编码值。
     * @param exposureDelayTicks 曝光延时节拍数。
     * @return 参数合法、命令服务存在且串口已打开时返回 true。
     */
    Q_INVOKABLE bool sendExposureCommand(bool freeRun, int frameFrequencyCode,
                                         int exposureDelayTicks);

    /**
     * @brief 发送伺服距离/速度修正联调命令。
     * @param distanceValid 距离修正量是否有效。
     * @param speedValid 速度修正量是否有效。
     * @param distanceXArcsec X 方向距离修正，单位角秒。
     * @param distanceYArcsec Y 方向距离修正，单位角秒。
     * @param speedXArcsecPerSec X 方向速度修正，单位角秒每秒。
     * @param speedYArcsecPerSec Y 方向速度修正，单位角秒每秒。
     * @param mode 伺服工作模式。
     * @return 参数合法、命令服务存在且串口已打开时返回 true。
     */
    Q_INVOKABLE bool sendServoCorrection(bool distanceValid, bool speedValid,
                                         double distanceXArcsec, double distanceYArcsec,
                                         double speedXArcsecPerSec, double speedYArcsecPerSec,
                                         int mode);

    /**
     * @brief 发送主控状态回包联调命令。
     * @param year 年。
     * @param month 月。
     * @param day 日。
     * @param hour 时。
     * @param minute 分。
     * @param second 秒。
     * @param millisecond 毫秒。
     * @param azimuthDegrees 方位角，单位度。
     * @param elevationDegrees 俯仰角，单位度。
     * @param distanceValid 距离修正量是否有效。
     * @param speedValid 速度修正量是否有效。
     * @param distanceXArcsec X 方向距离修正，单位角秒。
     * @param distanceYArcsec Y 方向距离修正，单位角秒。
     * @param speedXArcsecPerSec X 方向速度修正，单位角秒每秒。
     * @param speedYArcsecPerSec Y 方向速度修正，单位角秒每秒。
     * @param servoMode 伺服工作模式。
     * @return 参数合法、命令服务存在且串口已打开时返回 true。
     */
    Q_INVOKABLE bool sendMasterControlStatus(int year, int month, int day, int hour, int minute,
                                             int second, int millisecond, double azimuthDegrees,
                                             double elevationDegrees, bool distanceValid,
                                             bool speedValid, double distanceXArcsec,
                                             double distanceYArcsec, double speedXArcsecPerSec,
                                             double speedYArcsecPerSec, int servoMode);

Q_SIGNALS:
    /**
     * @brief 所有串口通道状态变化。
     */
    void serialChannelsChanged();

    /**
     * @brief 单个串口通道打开状态变化。
     * @param key 串口通道键名。
     * @param isOpen 是否已打开。
     */
    void serialChannelStateChanged(const QString& key, bool isOpen);

    /**
     * @brief 串口模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
};

}  // namespace Dss::Ui
