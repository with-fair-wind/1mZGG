#include "dss/ui/app_event.h"

namespace Dss::Ui {

AppEvent::AppEvent(QObject* parent) : QObject(parent) {}

auto AppEvent::instance() -> AppEvent& {
    static AppEvent event;
    return event;
}

void AppEvent::publishTargetPositionSelected(QPointF position) {
    Q_EMIT targetPositionSelected(position);
}

void AppEvent::publishZoomLevelChanged(int level) {
    Q_EMIT zoomLevelChanged(level);
}

}  // namespace Dss::Ui
