#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <optional>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 跟踪子 ViewModel，负责跟踪模式、手动目标和跟踪结果展示。
 */
class TrackingViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int trackMode READ trackMode WRITE setTrackMode NOTIFY trackModeChanged)

public:
    /**
     * @brief 构造跟踪子 ViewModel 并订阅跟踪结果事件。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit TrackingViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构跟踪子 ViewModel，并释放事件订阅连接。
     */
    ~TrackingViewModel() override;

    /**
     * @brief 获取当前跟踪模式。
     * @return 当前跟踪模式枚举的整型值。
     */
    [[nodiscard]] int trackMode() const;

public Q_SLOTS:
    /**
     * @brief 设置跟踪模式并同步到 ImageProcessor。
     * @param mode 跟踪模式枚举的整型值。
     */
    Q_INVOKABLE void setTrackMode(int mode);

    /**
     * @brief 在图像坐标中选择手动跟踪目标。
     * @param pos 图像坐标。
     */
    Q_INVOKABLE void selectTarget(QPointF pos);

Q_SIGNALS:
    /**
     * @brief 跟踪模式变化。
     * @param mode 新跟踪模式枚举的整型值。
     */
    void trackModeChanged(int mode);

    /**
     * @brief 跟踪信息更新。
     * @param info 可显示的跟踪状态文本。
     */
    void trackInfoUpdated(const QString& info);

    /**
     * @brief 目标列表数量更新。
     * @param count 当前目标数量。
     */
    void targetListUpdated(int count);

    /**
     * @brief 跟踪模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 订阅跟踪结果事件。
     */
    void setupSubscriptions();

    /**
     * @brief 处理跟踪结果事件并转为 UI 信号。
     * @param event 跟踪结果事件。
     */
    void onTrackResult(const Dss::Core::TrackResultEvent& event);

    /**
     * @brief 根据当前跟踪模式配置 ImageProcessor 跟踪策略。
     */
    void configureTrackingStrategy();

    /**
     * @brief 由点击位置构造手动跟踪目标。
     * @param pos 图像坐标。
     * @return 可直接注入 ManualTracker 的测量目标。
     */
    [[nodiscard]] static auto makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob;

    UiServiceContext::MessageBus& m_bus;       ///< 应用事件总线。
    Dss::Core::ServiceRegistry& m_registry;    ///< 应用服务注册表。
    int m_trackMode = static_cast<int>(Dss::Core::TrackMode::Init);  ///< 当前跟踪模式。
    std::optional<Dss::Core::MeasuredBlob> m_manualTarget;  ///< 手动选定的跟踪目标。
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
