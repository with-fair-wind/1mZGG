#pragma once

#include <cstddef>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include "dss/core/events.h"
#include "dss/core/event_bus.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 日志页子 ViewModel，负责 UI 日志缓存、过滤与事件桥接。
 */
class LogViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int logMinimumLevel READ logMinimumLevel WRITE setLogMinimumLevel NOTIFY
                   logMinimumLevelChanged)

public:
    /**
     * @brief 构造日志子 ViewModel 并订阅日志相关事件。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit LogViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构日志子 ViewModel，并释放事件订阅连接。
     */
    ~LogViewModel() override;

    /**
     * @brief 获取日志页最小显示级别。
     * @return 日志级别整数值。
     */
    [[nodiscard]] int logMinimumLevel() const;

    /**
     * @brief 获取当前过滤后的日志文本快照。
     * @return 满足最小级别过滤条件的日志列表。
     */
    [[nodiscard]] QStringList visibleLogEntries() const;

public Q_SLOTS:
    /**
     * @brief 设置日志页最小显示级别。
     * @param level UI 传入的日志级别整数值。
     */
    void setLogMinimumLevel(int level);

Q_SIGNALS:
    /**
     * @brief 追加一条满足当前过滤条件的日志文本。
     * @param text 日志展示文本。
     */
    void logEntryAppended(const QString& text);

    /**
     * @brief 当前可见日志列表发生变化。
     * @param entries 过滤后的日志文本快照。
     */
    void logEntriesChanged(const QStringList& entries);

    /**
     * @brief 日志页最小显示级别发生变化。
     * @param level 新的日志级别整数值。
     */
    void logMinimumLevelChanged(int level);

    /**
     * @brief 日志事件要求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief UI 日志缓存项。
     */
    struct UiLogEntry {
        Dss::Core::LogLevel level = Dss::Core::LogLevel::Info;  ///< 日志级别。
        QString text;                                           ///< 展示文本。
    };

    static constexpr std::size_t kMaxLogEntries = 500;  ///< UI 日志缓存最大条数。

    /**
     * @brief 订阅日志、串口错误和网络错误事件。
     */
    void setupSubscriptions();

    /**
     * @brief 将网络发送失败事件同步到状态栏和日志页。
     * @param event 网络发送失败事件。
     */
    void onNetworkTransmissionError(const Dss::Core::NetworkTransmissionErrorEvent& event);

    /**
     * @brief 将串口帧校验失败事件转发到 UI 日志页。
     * @param event 串口帧错误事件。
     */
    void onSerialFrameError(const Dss::Core::SerialFrameErrorEvent& event);

    /**
     * @brief 将串口协议字段解码失败事件转发到 UI 日志页。
     * @param event 串口解码错误事件。
     */
    void onSerialDecodeError(const Dss::Core::SerialDecodeErrorEvent& event);

    /**
     * @brief 将核心日志事件转发到 UI 日志页。
     * @param event 核心日志事件。
     */
    void onLogMessage(const Dss::Core::LogMessageEvent& event);

    /**
     * @brief 追加一条 UI 日志并维护缓存容量。
     * @param level 日志级别。
     * @param text 日志展示文本。
     */
    void appendLogEntry(Dss::Core::LogLevel level, QString text);

    /**
     * @brief 判断日志级别是否满足当前 UI 过滤条件。
     * @param level 待判断日志级别。
     * @return 满足过滤条件时返回 true。
     */
    [[nodiscard]] bool isLogLevelVisible(Dss::Core::LogLevel level) const;

    UiServiceContext::MessageBus& m_bus;  ///< 应用事件总线。
    Dss::Core::LogLevel m_logMinimumLevel =
        Dss::Core::LogLevel::Info;                 ///< 日志页最小显示级别。
    std::vector<UiLogEntry> m_logEntries;          ///< UI 日志缓存。
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
