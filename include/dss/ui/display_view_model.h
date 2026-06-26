#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 显示子 ViewModel，负责显示拉伸、图像缓存与显示刷新事件。
 */
class DisplayViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool displayAutoStretch READ displayAutoStretch NOTIFY displayStretchSettingsChanged)
    Q_PROPERTY(int displayStretchLow READ displayStretchLow NOTIFY displayStretchSettingsChanged)
    Q_PROPERTY(int displayStretchHigh READ displayStretchHigh NOTIFY displayStretchSettingsChanged)

public:
    /**
     * @brief 构造显示子 ViewModel 并订阅显示与处理事件。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit DisplayViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构显示子 ViewModel，并释放事件订阅连接。
     */
    ~DisplayViewModel() override;

    /**
     * @brief 是否启用自动显示拉伸。
     * @return 自动拉伸开启时返回 true。
     */
    [[nodiscard]] bool displayAutoStretch() const;

    /**
     * @brief 获取手动显示拉伸低阈值。
     * @return 低阈值。
     */
    [[nodiscard]] int displayStretchLow() const;

    /**
     * @brief 获取手动显示拉伸高阈值。
     * @return 高阈值。
     */
    [[nodiscard]] int displayStretchHigh() const;

public Q_SLOTS:
    /**
     * @brief 通过事件总线发送缩放级别变化。
     * @param level 缩放级别。
     */
    Q_INVOKABLE void toggleZoom(int level);

    /**
     * @brief 应用显示拉伸设置并尝试刷新当前图像。
     * @param autoStretch 是否启用自动拉伸。
     * @param low 手动低阈值。
     * @param high 手动高阈值。
     * @return 参数合法且写入处理器成功时返回 true。
     */
    Q_INVOKABLE bool applyDisplayStretch(bool autoStretch, int low, int high);

    /**
     * @brief 设置显示自动拉伸开关。
     * @param autoStretch 是否启用自动拉伸。
     */
    Q_INVOKABLE void setDisplayAutoStretch(bool autoStretch);

    /**
     * @brief 设置显示拉伸低阈值。
     * @param low 低阈值。
     */
    Q_INVOKABLE void setDisplayStretchLow(int low);

    /**
     * @brief 设置显示拉伸高阈值。
     * @param high 高阈值。
     */
    Q_INVOKABLE void setDisplayStretchHigh(int high);

    /**
     * @brief 清除当前可重绘帧缓存。
     */
    void clearCurrentDisplayFrame();

Q_SIGNALS:
    /**
     * @brief 显示拉伸属性变化。
     */
    void displayStretchSettingsChanged();

    /**
     * @brief 显示拉伸设置变化。
     * @param autoStretch 是否启用自动拉伸。
     * @param low 手动低阈值。
     * @param high 手动高阈值。
     */
    void displayStretchChanged(bool autoStretch, int low, int high);

    /**
     * @brief 主显示图像就绪。
     * @param image 可显示的灰度图。
     */
    void displayImageReady(QImage image);

    /**
     * @brief 图像统计量更新。
     * @param minVal 最小值。
     * @param maxVal 最大值。
     * @param avg 均值。
     * @param stdDev 标准差。
     */
    void imageStatsUpdated(double minVal, double maxVal, double avg, double stdDev);

    /**
     * @brief 显示模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 订阅显示刷新和处理完成事件。
     */
    void setupSubscriptions();

    /**
     * @brief 处理显示刷新事件。
     * @param event 显示刷新事件。
     */
    void onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event);

    /**
     * @brief 处理图像处理完成事件。
     * @param event 图像处理完成事件。
     */
    void onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event);

    /**
     * @brief 缓存最近一次显示事件中的 RAW 帧。
     * @param event 显示刷新事件。
     */
    void cacheCurrentDisplayFrame(const Dss::Core::DisplayRefreshEvent& event);

    /**
     * @brief 使用当前拉伸设置重建并发送当前显示图像。
     * @return 刷新成功时返回 true。
     */
    [[nodiscard]] bool refreshCurrentDisplayFromStretch();

    /**
     * @brief 将当前显示拉伸设置同步到图像处理器。
     * @return 同步成功时返回 true。
     */
    [[nodiscard]] bool syncDisplayStretchToProcessor();

    UiServiceContext::MessageBus& m_bus;  ///< 应用事件总线。
    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    bool m_displayAutoStretch = true;        ///< 是否启用显示自动拉伸。
    int m_displayStretchLow = 1000;          ///< 显示拉伸低阈值。
    int m_displayStretchHigh = 5000;         ///< 显示拉伸高阈值。
    mutable std::mutex m_currentDisplayMutex;  ///< 保护当前显示帧缓存。
    std::shared_ptr<const std::vector<std::uint16_t>> m_currentRawImage;  ///< 当前 RAW 图像。
    std::uint64_t m_currentDisplayFrameSeq = 0;  ///< 当前显示帧序号。
    std::uint32_t m_currentDisplayWidth = 0;     ///< 当前显示帧宽度。
    std::uint32_t m_currentDisplayHeight = 0;    ///< 当前显示帧高度。
    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
