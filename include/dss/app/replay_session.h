#pragma once

#include <condition_variable>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "dss/core/event/message_bus.h"

namespace Dss::Acquisition {
class ImageSequenceFrameSource;
class FrameSourceCoordinator;
}  // namespace Dss::Acquisition
namespace Dss::Processing {
class ImageProcessor;
}  // namespace Dss::Processing

namespace Dss::App {

/**
 * @brief 串行编排文件回放的应用服务，不依赖 Qt 事件循环。
 * @details 命令只登记意图；文件读取、帧源启停、处理器 drain/reset 均在服务线程执行。
 * Loading/Stopping 时拒绝普通命令，stop 可取消在途命令。暂停保留历史，定位和换序列
 * 清理历史；EOF 后 start 回到首帧，失败后 start/step 重新加载序列。
 * @note bus 必须比本对象长寿。服务独占依赖对象的回放生命周期操作；其他调用者不得
 * 并发启停/reset 同一帧源或处理器。shutdown 必须由外部所有者调用，不能从工作线程回调调用。
 */
class ReplaySession final {
public:
    /// 会话状态；Closed 为不可恢复的关闭状态。
    enum class State { Idle, Loading, Running, Stopping, Completed, Failed, Closed };
    /// 供展示层映射文本的业务结果，不携带 Qt 类型。
    enum class Notice {
        None,
        Loading,
        Selected,
        Playing,
        Stopping,
        Stopped,
        Ready,
        Positioned,
        Finished,
        Error
    };
    /// 在同一锁内取得的一致快照；终态发布前已完成源停止和处理器回收。
    struct Snapshot {
        State state = State::Idle;     ///< 当前生命周期状态。
        Notice notice = Notice::None;  ///< 最近状态的展示含义。
        std::uint64_t revision = 0;    ///< 每次状态发布递增，供界面去重。
        std::size_t frameCount = 0;    ///< 已完成登记的序列帧数。
        std::size_t position = 0;      ///< Positioned 通知的零基定位索引。
        std::string error;             ///< Error 通知的失败原因。

        /// @brief 是否仍在执行命令或回收资源。
        /// @return Loading/Stopping 为 true，其他状态为 false。
        [[nodiscard]] bool busy() const {
            return state == State::Loading || state == State::Stopping;
        }
    };

    /**
     * @brief 创建服务及其串行工作线程。
     * @param bus 借用的事件总线，发布 GrabStarted/GrabStopped。
     * @param source 非空回放帧源。
     * @param processor 非空处理器。
     * @param coordinator 可选帧源协调器，回放前切换至 Replay。
     * @throws std::invalid_argument source 或 processor 为空。
     */
    ReplaySession(Dss::Core::MessageBus& bus,
                  std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource> source,
                  std::shared_ptr<Dss::Processing::ImageProcessor> processor,
                  std::shared_ptr<Dss::Acquisition::FrameSourceCoordinator> coordinator = {});
    /// 取消并 join；可能等待当前解码或处理回调返回。
    ~ReplaySession();

    /// 异步替换非空序列并清理历史；成功仅表示接受，完成由 snapshot 表示。
    /// @param files 按播放顺序排列的非空路径列表。
    /// @return 接受成功或拒绝原因；解码错误由后续快照报告。
    [[nodiscard]] auto selectFiles(std::vector<std::filesystem::path> files)
        -> std::expected<void, std::string>;
    /// 异步初始化并连续播放；忙碌、已运行或关闭时拒绝。
    /// @return 接受成功或拒绝原因。
    [[nodiscard]] auto start() -> std::expected<void, std::string>;
    /// 异步处理一帧并 drain；backward 时先清理历史并定位上一显示帧。
    /// @param backward true 表示后退一帧，false 表示前进一帧。
    /// @return 接受成功或拒绝原因。
    [[nodiscard]] auto step(bool backward = false) -> std::expected<void, std::string>;
    /// 异步定位到零基索引并清理历史；若原先连续运行，完成后恢复播放。
    /// @param index 小于序列帧数的零基索引。
    /// @return 接受成功或拒绝原因。
    [[nodiscard]] auto seek(std::size_t index) -> std::expected<void, std::string>;
    /// 非阻塞请求停止：取消加载/单步，连续回放停止后 drain，保留处理历史。
    void stop();
    /// 线程安全地取得当前状态；不执行文件、线程 join 或处理器操作。
    /// @return 同一互斥锁内复制的会话状态。
    [[nodiscard]] auto snapshot() const -> Snapshot;
    /// 永久拒绝新命令，取消并等待服务线程及源/处理器退出；幂等，可阻塞。
    void shutdown();

private:
    /// 单槽工作指令，Stop 使用独立优先标志，避免被普通命令覆盖。
    enum class Operation { Select, Start, Forward, Backward, Seek };
    /// 接收命令时冻结的参数与恢复意图。
    struct Command {
        Operation operation;                       ///< 操作类型。
        std::vector<std::filesystem::path> files;  ///< Select 的新序列。
        std::size_t index = 0;                     ///< Seek 的零基索引。
        bool resume = false;                       ///< 定位前是否运行。
        bool recover = false;                      ///< 是否从失败状态重新加载。
    };
    /// 锁内校验和提交单槽命令，不调用依赖服务。
    /// @param command 待接受的指令与参数。
    /// @return 接受成功或拒绝原因。
    auto submit(Command command) -> std::expected<void, std::string>;
    /// 等待命令；仅 Running 时周期检查源 EOF/处理失败。
    void run();
    /// 在服务线程执行命令；错误交由 run 收口。
    /// @param command 已接受的指令。
    /// @param token 当前指令的取消令牌。
    void execute(Command command, std::stop_token token);
    /// 停止全部相关源，保证随后 drain/reset 不再有生产者。
    void stopSources();
    /// 连续运行前检查取消并发布开始事件。
    /// @param token 当前启动操作的取消令牌。
    void beginPlayback(std::stop_token token);
    /// 最多发布一次与开始配对的停止事件。
    void notifyStopped();
    /// 发布结果；停止/关闭请求已生效时拒绝旧任务的状态覆盖。
    /// @param state 要发布的状态。
    /// @param notice 对应的业务通知。
    /// @param error 失败原因，成功时为空。
    /// @param position 定位通知的零基索引。
    void publish(State state, Notice notice, std::string error = {}, std::size_t position = 0);

    Dss::Core::MessageBus& m_bus;  ///< 借用总线，外部保证生命周期。
    std::shared_ptr<Dss::Acquisition::ImageSequenceFrameSource> m_source;     ///< 回放源。
    std::shared_ptr<Dss::Processing::ImageProcessor> m_processor;             ///< 处理器。
    std::shared_ptr<Dss::Acquisition::FrameSourceCoordinator> m_coordinator;  ///< 可选协调器。
    mutable std::mutex m_mutex;                  ///< 保护快照、指令、关闭/停止标志和取消源。
    std::condition_variable m_wake;              ///< 命令与停止唤醒。
    Snapshot m_snapshot;                         ///< 最近发布的一致快照。
    std::optional<Command> m_pending;            ///< 有界单槽命令。
    std::stop_source m_cancel;                   ///< 当前命令的协作取消源。
    bool m_stopRequested = false;                ///< 优先执行的停止意图。
    bool m_closed = false;                       ///< 已拒绝所有新命令。
    bool m_announcedRunning = false;             ///< 仅工作线程使用的开始/停止事件配对标志。
    std::vector<std::filesystem::path> m_files;  ///< 仅工作线程访问的失败恢复序列。
    std::mutex m_shutdownMutex;                  ///< 串行化外部 shutdown/join。
    std::jthread m_worker;                       ///< 最后创建、最先回收的服务线程。
};

}  // namespace Dss::App
