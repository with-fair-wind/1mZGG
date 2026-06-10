#pragma once

#include <QObject>
#include <QPointF>

namespace Dss::Ui {

/// 全局 UI 事件单例，用于跨控件解耦通信
class AppEvent final : public QObject {
    Q_OBJECT

public:
    /// 获取全局单例实例
    [[nodiscard]] static auto instance() -> AppEvent&;

public slots:
    /// 发布目标位置选中事件
    void publishTargetPositionSelected(QPointF position);
    /// 发布缩放级别变化事件
    void publishZoomLevelChanged(int level);

signals:
    /// 用户在图像上选中了目标位置
    void targetPositionSelected(QPointF position);
    /// 缩放级别已变化
    void zoomLevelChanged(int level);

private:
    explicit AppEvent(QObject* parent = nullptr);
};

}  // namespace Dss::Ui
