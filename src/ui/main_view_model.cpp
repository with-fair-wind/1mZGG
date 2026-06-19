#include "dss/ui/main_view_model.h"

#include <utility>

namespace Dss::Ui {

MainViewModel::MainViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry, QObject* parent)
    : QObject(parent),
      m_bus(bus),
      m_registry(registry),
      m_logs(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_replay(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_display(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_processing(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_tracking(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_storage(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_settings(Dss::Core::Config::instance(), this),
      m_serialPorts(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_network(UiServiceContext{.bus = m_bus, .registry = m_registry}, this),
      m_dataExchange(UiServiceContext{.bus = m_bus, .registry = m_registry}, this) {
    connectChildViewModels();
    setupSubscriptions();
}

MainViewModel::~MainViewModel() = default;

auto MainViewModel::statusText() const -> QString {
    return m_statusText;
}

double MainViewModel::exposure() const {
    return m_exposure;
}

auto MainViewModel::logs() -> LogViewModel& {
    return m_logs;
}

auto MainViewModel::logs() const -> const LogViewModel& {
    return m_logs;
}

auto MainViewModel::replay() -> ReplayViewModel& {
    return m_replay;
}

auto MainViewModel::replay() const -> const ReplayViewModel& {
    return m_replay;
}

auto MainViewModel::display() -> DisplayViewModel& {
    return m_display;
}

auto MainViewModel::display() const -> const DisplayViewModel& {
    return m_display;
}

auto MainViewModel::processing() -> ProcessingViewModel& {
    return m_processing;
}

auto MainViewModel::processing() const -> const ProcessingViewModel& {
    return m_processing;
}

auto MainViewModel::tracking() -> TrackingViewModel& {
    return m_tracking;
}

auto MainViewModel::tracking() const -> const TrackingViewModel& {
    return m_tracking;
}

auto MainViewModel::storage() -> StorageViewModel& {
    return m_storage;
}

auto MainViewModel::storage() const -> const StorageViewModel& {
    return m_storage;
}
auto MainViewModel::settings() -> SettingsViewModel& {
    return m_settings;
}

auto MainViewModel::settings() const -> const SettingsViewModel& {
    return m_settings;
}

auto MainViewModel::serialPorts() -> SerialPortViewModel& {
    return m_serialPorts;
}

auto MainViewModel::serialPorts() const -> const SerialPortViewModel& {
    return m_serialPorts;
}

auto MainViewModel::network() -> NetworkViewModel& {
    return m_network;
}

auto MainViewModel::network() const -> const NetworkViewModel& {
    return m_network;
}

auto MainViewModel::dataExchange() -> DataExchangeViewModel& {
    return m_dataExchange;
}

auto MainViewModel::dataExchange() const -> const DataExchangeViewModel& {
    return m_dataExchange;
}

void MainViewModel::setExposure(double ms) {
    if (m_exposure == ms) {
        return;
    }
    m_exposure = ms;
    Q_EMIT exposureChanged(ms);
}

void MainViewModel::connectChildViewModels() {
    auto forwardStatus = [this](const QString& text) { setStatus(text); };

    connect(&m_logs, &LogViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_replay, &ReplayViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_display, &DisplayViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_processing, &ProcessingViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_tracking, &TrackingViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_storage, &StorageViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_settings, &SettingsViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_serialPorts, &SerialPortViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_network, &NetworkViewModel::statusTextChanged, this, forwardStatus);
    connect(&m_dataExchange, &DataExchangeViewModel::statusTextChanged, this, forwardStatus);

    connect(&m_network, &NetworkViewModel::dataExchangeEndpointEdited, &m_dataExchange,
            &DataExchangeViewModel::closeForEndpointReconfigure);
}

void MainViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::MasterControlEvent>(
        [this](const Dss::Core::MasterControlEvent& event) { onMasterControl(event); }));
}

void MainViewModel::onMasterControl(const Dss::Core::MasterControlEvent& event) {
    setExposure(event.exposure);
    m_tracking.setTrackMode(event.trackMode);

    if (event.save && !m_storage.isSaving()) {
        m_storage.startSaving();
    } else if (!event.save && m_storage.isSaving()) {
        m_storage.stopSaving();
    }

    if (event.grab && !m_replay.isGrabbing()) {
        m_display.clearCurrentDisplayFrame();
        m_replay.startGrab();
    } else if (!event.grab && m_replay.isGrabbing()) {
        m_replay.stopGrab();
    }
}

void MainViewModel::setStatus(QString text) {
    if (m_statusText == text) {
        return;
    }
    m_statusText = std::move(text);
    Q_EMIT statusTextChanged(m_statusText);
}

}  // namespace Dss::Ui
