#pragma once

#include <QString>
#include <QStringList>
#include <QObject>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 回放子 ViewModel，负责图像序列选择、播放控制与帧进度状态。
 */
class ReplayViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isGrabbing READ isGrabbing NOTIFY grabbingChanged)
    Q_PROPERTY(int replayFrameCount READ replayFrameCount NOTIFY replayFrameCountChanged)
    Q_PROPERTY(int replayCurrentFrame READ replayCurrentFrame NOTIFY replayCurrentFrameChanged)

public:
    /**
     * @brief 构造回放子 ViewModel 并订阅帧刷新事件。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit ReplayViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构回放子 ViewModel，并释放事件订阅连接。
     */
    ~ReplayViewModel() override;

    /**
     * @brief 当前是否正在连续采集或回放。
     * @return 正在运行时返回 true。
     */
    [[nodiscard]] bool isGrabbing() const;

    /**
     * @brief 获取当前序列总帧数。
     * @return 回放序列总帧数。
     */
    [[nodiscard]] int replayFrameCount() const;

    /**
     * @brief 获取当前回放进度帧号。
     * @return 当前帧号，显示为一基序号。
     */
    [[nodiscard]] int replayCurrentFrame() const;

public Q_SLOTS:
    /**
     * @brief 选择回放图像序列文件。
     * @param files 图像文件路径列表。
     * @return 成功返回 true，失败返回 false。
     */
    Q_INVOKABLE bool selectReplayFiles(const QStringList& files);

    /**
     * @brief 开始连续采集或回放。
     */
    Q_INVOKABLE void startGrab();

    /**
     * @brief 停止连续采集或回放。
     */
    Q_INVOKABLE void stopGrab();

    /**
     * @brief 单步前进回放一帧。
     * @return 成功返回 true，失败返回 false。
     */
    Q_INVOKABLE bool stepReplayForward();

Q_SIGNALS:
    /**
     * @brief 采集或回放运行状态变化。
     * @param value 正在运行时为 true。
     */
    void grabbingChanged(bool value);

    /**
     * @brief 回放序列总帧数变化。
     * @param count 新的总帧数。
     */
    void replayFrameCountChanged(int count);

    /**
     * @brief 当前回放帧号变化。
     * @param frame 新的当前帧号。
     */
    void replayCurrentFrameChanged(int frame);

    /**
     * @brief 回放模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 订阅显示刷新事件，用于同步当前帧进度。
     */
    void setupSubscriptions();

    /**
     * @brief 处理显示刷新事件。
     * @param event 显示刷新事件。
     */
    void onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event);

    /**
     * @brief 更新当前帧号并发送变更信号。
     * @param frame 新的当前帧号。
     */
    void setReplayCurrentFrame(int frame);

    /**
     * @brief 更新总帧数并发送变更信号。
     * @param count 新的总帧数。
     */
    void setReplayFrameCount(int count);

    /**
     * @brief 更新回放运行状态并发送变更信号。
     * @param value 新的运行状态。
     */
    void setGrabbing(bool value);

    UiServiceContext::MessageBus& m_bus;               ///< 应用事件总线。
    Dss::Core::ServiceRegistry& m_registry;            ///< 应用服务注册表。
    bool m_grabbing = false;                           ///< 是否正在采集或回放。
    int m_replayFrameCount = 0;                        ///< 回放序列总帧数。
    int m_replayCurrentFrame = 0;                      ///< 当前回放帧号。
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
