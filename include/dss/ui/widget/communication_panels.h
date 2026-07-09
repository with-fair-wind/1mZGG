#pragma once

class QWidget;

namespace Dss::Ui {
class DataExchangeViewModel;
class NetworkViewModel;
class SerialPortViewModel;

[[nodiscard]] auto createSerialChannelsPanel(SerialPortViewModel& viewModel, QWidget* parent)
    -> QWidget*;
[[nodiscard]] auto createSerialCommandsPanel(SerialPortViewModel& viewModel, QWidget* parent)
    -> QWidget*;
[[nodiscard]] auto createNetworkEndpointsPanel(NetworkViewModel& network,
                                               DataExchangeViewModel& dataExchange, QWidget* parent)
    -> QWidget*;

}  // namespace Dss::Ui
