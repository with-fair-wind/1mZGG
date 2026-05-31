#pragma once

#include <QObject>
#include <QPointF>

namespace Dss::Ui
{

class AppEvent final : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static auto instance() -> AppEvent&;

public slots:
    void publishTargetPositionSelected(QPointF position);
    void publishZoomLevelChanged(int level);

signals:
    void targetPositionSelected(QPointF position);
    void zoomLevelChanged(int level);

private:
    explicit AppEvent(QObject* parent = nullptr);
};

} // namespace Dss::Ui
