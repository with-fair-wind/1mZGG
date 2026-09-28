#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "dss/core/event/event_bus.h"
#include "dss/core/event/events.h"
#include "dss/ui/view_model/view_model_context.h"

namespace Dss::App {
class ReplaySession;
}

namespace Dss::Ui {

/**
 * @brief 回放展示适配器，向 ReplaySession 提交命令并映射 Qt 属性、文本和帧进度。
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

    /// UI 线程关闭入口：取消并 join 后台任务，再停止帧源/处理器；不发 UI 信号，幂等。
    void shutdown();

    /**
     * @brief 当前是否正在连续采集或回放。
     * @return 正在运行时返回 true。
     */
    [[nodiscard]] bool isGrabbing() const;

    /**
     * @brief 当前是否正在执行选择、加载、定位、单步或停止回收。
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
     * @brief 异步登记回放序列并清理旧历史；完成后更新计数，不解码图像。
     * @param files 图像文件路径列表。
     * @return 命令已接受时返回 true；完成由 replayBusyChanged(false) 表示。
     */
    Q_INVOKABLE bool selectReplayFiles(const QStringList& files);

    /**
     * @brief 提交回放源初始化任务，成功后开始连续回放。
     */
    Q_INVOKABLE void startGrab();

    /**
     * @brief 异步停止回放并等待已接受帧处理完成，期间 replayBusy 为 true。
     */
    Q_INVOKABLE void stopGrab();
    /// @brief 取消加载或单步任务并异步停止。
    /// @note 兼容主控停止入口，与 stopGrab 使用同一服务停止操作。
    void requestStopAfterLoad();

    /**
     * @brief 提交单步前进回放一帧的后台任务。
     * @return 任务成功提交时返回 true；服务缺失或已有任务运行时返回 false。
     */
    Q_INVOKABLE bool stepReplayForward();

    /** @brief 提交单步回放上一帧的后台任务。 @return 任务成功提交时返回 true。 */
    Q_INVOKABLE bool stepReplayBackward();

    /**
     * @brief 异步将下一帧定位到零基索引并清理历史。
     * @param index 目标帧索引。
     * @return 命令已接受时返回 true，完成前拒绝其他回放命令。
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
    /// 检查服务是否可用及 UI 是否仍有未消费的命令结果。
    /// @return 可提交普通命令时为 true。
    [[nodiscard]] bool canSubmit();
    /// 映射命令接收结果，成功时立即置 busy，等待后续快照确认完成。
    /// @param result 应用服务的命令接收结果。
    /// @return 命令被接受时为 true。
    bool acceptCommand(const std::expected<void, std::string>& result);
    /// 在对象线程映射应用层快照；不启停或等待后端服务。
    void refreshSession();

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
    bool m_shutdown = false;                                      ///< 已关闭，忽略后续排队通知。
    std::atomic<std::uint64_t> m_sessionRevision{0};              ///< 过滤旧会话的已投递进度更新。
    bool m_replayBusy = false;                                    ///< 是否正在执行回放后台任务。
    std::shared_ptr<Dss::App::ReplaySession> m_session;           ///< 应用层持有的回放服务。
    std::uint64_t m_snapshotRevision = 0;                         ///< 最近映射的服务快照版本。
    int m_replayFrameCount = 0;                                   ///< 回放序列总帧数。
    int m_replayCurrentFrame = 0;                                 ///< 当前回放帧号。
    QString m_runtimeDiagnosticsText{"Diagnostics unavailable"};  ///< 当前诊断摘要文本
    std::vector<Dss::Evt::ScopedConnection> m_connections;        ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
