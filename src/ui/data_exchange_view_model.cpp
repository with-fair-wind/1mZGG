#include "dss/ui/data_exchange_view_model.h"

#include "dss/core/config.h"
#include "dss/network/data_exchange.h"
#include "dss/ui/network_endpoint_helpers.h"

namespace Dss::Ui {

DataExchangeViewModel::DataExchangeViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

DataExchangeViewModel::~DataExchangeViewModel() = default;

bool DataExchangeViewModel::isDataExchangeOpen() const {
    return m_dataExchangeOpen;
}

auto DataExchangeViewModel::dataExchangeGxtcLocalIp() const -> QString {
    return endpointLocalIp(Dss::Core::Config::instance().commNet().exchangeGxtc);
}

int DataExchangeViewModel::dataExchangeGxtcLocalPort() const {
    return Dss::Core::Config::instance().commNet().exchangeGxtc.localPort;
}

auto DataExchangeViewModel::dataExchangeGxtcRemoteIp() const -> QString {
    return endpointRemoteIp(Dss::Core::Config::instance().commNet().exchangeGxtc);
}

int DataExchangeViewModel::dataExchangeGxtcRemotePort() const {
    return Dss::Core::Config::instance().commNet().exchangeGxtc.remotePort;
}

auto DataExchangeViewModel::dataExchangeGdclLocalIp() const -> QString {
    return endpointLocalIp(Dss::Core::Config::instance().commNet().exchangeGdcl);
}

int DataExchangeViewModel::dataExchangeGdclLocalPort() const {
    return Dss::Core::Config::instance().commNet().exchangeGdcl.localPort;
}

auto DataExchangeViewModel::dataExchangeGdclRemoteIp() const -> QString {
    return endpointRemoteIp(Dss::Core::Config::instance().commNet().exchangeGdcl);
}

int DataExchangeViewModel::dataExchangeGdclRemotePort() const {
    return Dss::Core::Config::instance().commNet().exchangeGdcl.remotePort;
}

void DataExchangeViewModel::closeForEndpointReconfigure() {
    if (auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
        dataExchange && dataExchange->isOpen()) {
        dataExchange->close();
    }
    setDataExchangeOpen(false);
}

bool DataExchangeViewModel::applyDataExchangeEndpoints(
    const QString& gxtcLocalIp, int gxtcLocalPort, const QString& gxtcRemoteIp, int gxtcRemotePort,
    const QString& gdclLocalIp, int gdclLocalPort, const QString& gdclRemoteIp,
    int gdclRemotePort) {
    if (!isLocalUdpPortInRange(gxtcLocalPort) || !isLocalUdpPortInRange(gdclLocalPort)) {
        Q_EMIT statusTextChanged("Data exchange local ports must be 0-65535");
        return false;
    }
    if (!isRemoteUdpPortInRange(gxtcRemotePort) || !isRemoteUdpPortInRange(gdclRemotePort)) {
        Q_EMIT statusTextChanged("Data exchange remote ports must be 1-65535");
        return false;
    }

    auto& commNet = Dss::Core::Config::instance().mutableCommNet();
    commNet.exchangeGxtc =
        makeUdpEndpointConfig(gxtcLocalIp, gxtcLocalPort, gxtcRemoteIp, gxtcRemotePort);
    commNet.exchangeGdcl =
        makeUdpEndpointConfig(gdclLocalIp, gdclLocalPort, gdclRemoteIp, gdclRemotePort);
    commNet.exchange = commNet.exchangeGxtc;

    closeForEndpointReconfigure();

    Q_EMIT dataExchangeEndpointsChanged();
    Q_EMIT statusTextChanged("Data exchange endpoints applied");
    return true;
}

bool DataExchangeViewModel::openDataExchange() {
    auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
    if (!dataExchange) {
        setDataExchangeOpen(false);
        Q_EMIT statusTextChanged("Data exchange service is not registered");
        return false;
    }

    const auto& commNet = Dss::Core::Config::instance().commNet();
    auto result = dataExchange->open(commNet.exchangeGxtc, commNet.exchangeGdcl);
    if (!result.has_value()) {
        dataExchange->close();
        setDataExchangeOpen(false);
        Q_EMIT statusTextChanged(QString::fromStdString(result.error()));
        return false;
    }

    setDataExchangeOpen(dataExchange->isOpen());
    if (!m_dataExchangeOpen) {
        dataExchange->close();
        Q_EMIT statusTextChanged("Data exchange UDP open failed");
        return false;
    }

    Q_EMIT statusTextChanged("Data exchange UDP opened");
    return true;
}

void DataExchangeViewModel::closeDataExchange() {
    auto dataExchange = m_registry.tryGet<Dss::Network::DataExchange>("data_exchange");
    if (dataExchange) {
        dataExchange->close();
    }
    setDataExchangeOpen(false);
    Q_EMIT statusTextChanged("Data exchange UDP closed");
}

void DataExchangeViewModel::setDataExchangeOpen(bool value) {
    if (m_dataExchangeOpen == value) {
        return;
    }
    m_dataExchangeOpen = value;
    Q_EMIT dataExchangeOpenChanged(value);
}

}  // namespace Dss::Ui
