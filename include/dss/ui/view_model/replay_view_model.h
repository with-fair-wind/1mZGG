#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/ui/view_model/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 回放子 ViewModel，负责图像序列选择、播放控制与帧进度状态。
 */
class ReplayViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isGrabbing READ isGrabbing NOTIFY grabbingChanged)
    Q_PROPERTY(bool replayBusy READ replayBusy NOTIFY replayBusyChanged)
    Q_PROPERTY(int replayFrameCount READ replayFrameCount NOTIFY replayFrameCountChanged)
    Q_PROPERTY(int replayCurrentFrame READ replayCurrentFrame NOTIFY replayCurrentFrameChanged)
    Q_PROPERTY(QString runtimeDiagnosticsText READ runtimeDiagnosticsText NOTIFY
                   runtimeDiagnosticsTextChanged)

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
     * @brief 当前是否正在执行回放文件加载或单步后台任务。
     * @return 后台任务尚未完成时返回 true。
     */
    [[nodiscard]] bool replayBusy() const;

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

    /**
     * @brief 获取运行期诊断摘要。
     * @return 供界面展示的诊断文本。
     */
    [[nodiscard]] auto runtimeDiagnosticsText() const -> QString;

public Q_SLOTS:
    /**
     * @brief 提交选择回放图像序列文件的后台任务。
     * @param files 图像文件路径列表。
     * @return 任务成功提交时返回 true；服务缺失、路径无效或已有任务运行时返回 false。
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
     * @brief 提交单步前进回放一帧的后台任务。
     * @return 任务成功提交时返回 true；服务缺失或已有任务运行时返回 false。
     */
    Q_INVOKABLE bool stepReplayForward();

    /** @brief 提交单步回放上一帧的后台任务。 @return 任务成功提交时返回 true。 */
    Q_INVOKABLE bool stepReplayBackward();

    /**
     * @brief 将下一帧定位到零基索引。
     * @param index 目标帧索引。
     * @return 索引有效且定位成功时返回 true。
     */
    Q_INVOKABLE bool seekReplayFrame(int index);

    /// @brief 从运行服务重新读取并更新诊断摘要。
    Q_INVOKABLE void refreshRuntimeDiagnostics();

Q_SIGNALS:
    /**
     * @brief 采集或回放运行状态变化。
     * @param value 正在运行时为 true。
     */
    void grabbingChanged(bool value);

    /**
     * @brief 回放后台任务运行状态变化。
     * @param value 后台加载或单步任务运行时为 true。
     */
    void replayBusyChanged(bool value);

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
     * @brief 运行期诊断摘要变化。
     * @param text 新的诊断文本。
     */
    void runtimeDiagnosticsTextChanged(const QString& text);

    /**
     * @brief 回放模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 回放后台任务完成后回到 UI 线程应用的结果。
     */
    struct ReplayTaskResult {
        bool success = false;             ///< 任务是否成功完成。
        std::optional<int> frameCount;    ///< 需要更新的序列总帧数。
        std::optional<int> currentFrame;  ///< 需要更新的当前帧号。
        QString statusText;               ///< 任务完成后展示的状态文本。
    };

    using ReplayTask = std::function<ReplayTaskResult(std::stop_token)>;  ///< 后台回放任务类型。

    /**
     * @brief 启动一个串行回放后台任务。
     * @param loadingText 任务开始时显示的状态文本。
     * @param task 在 std::jthread 中执行的任务体。
     * @return 任务成功提交时返回 true。
     */
    [[nodiscard]] bool startReplayTask(const QString& loadingText, ReplayTask task);

    /**
     * @brief 在 UI 线程应用回放后台任务完成结果。
     * @param result 后台任务产生的结果。
     */
    void finishReplayTask(const ReplayTaskResult& result);

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

    /**
     * @brief 更新回放后台任务运行状态并发送变更信号。
     * @param value 新的后台任务运行状态。
     */
    void setReplayBusy(bool value);

    UiServiceContext::MessageBus& m_bus;                          ///< 应用事件总线。
    Dss::Core::ServiceRegistry& m_registry;                       ///< 应用服务注册表。
    bool m_grabbing = false;                                      ///< 是否正在采集或回放。
    bool m_replayBusy = false;                                    ///< 是否正在执行回放后台任务。
    std::jthread m_replayTaskWorker;                              ///< 回放文件加载与单步 worker。
    int m_replayFrameCount = 0;                                   ///< 回放序列总帧数。
    int m_replayCurrentFrame = 0;                                 ///< 当前回放帧号。
    QString m_runtimeDiagnosticsText{"Diagnostics unavailable"};  ///< 当前诊断摘要文本
    std::vector<Dss::Evt::ScopedConnection> m_connections;        ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
