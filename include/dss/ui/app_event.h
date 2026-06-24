#pragma once

#include <QObject>
#include <QPointF>

namespace Dss::Ui {

/// 全局 UI 事件单例，用于跨控件解耦通信
class AppEvent final : public QObject {
    Q_OBJECT

public:
    /** @brief 获取全局 UI 事件实例。 @return 进程级事件对象的引用。 */
    [[nodiscard]] static auto instance() -> AppEvent&;

public slots:
    /**
     * @brief 发布目标位置选中事件。
     * @param position 图像坐标系中的目标位置。
     */
    void publishTargetPositionSelected(QPointF position);
    /**
     * @brief 发布缩放级别变化事件。
     * @param level 新的缩放级别。
     */
    void publishZoomLevelChanged(int level);

signals:
    /**
     * @brief 用户在图像上选中了目标位置。
     * @param position 图像坐标系中的目标位置。
     */
    void targetPositionSelected(QPointF position);
    /**
     * @brief 缩放级别已变化。
     * @param level 新的缩放级别。
     */
    void zoomLevelChanged(int level);

private:
    /**
     * @brief 创建全局 UI 事件对象。
     * @param parent Qt 父对象。
     */
    explicit AppEvent(QObject* parent = nullptr);
};

}  // namespace Dss::Ui
