#pragma once

#include <QObject>
#include <QString>
#include <string>
#include <vector>

#include "dss/core/event_bus.h"
#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/ui/data_exchange_view_model.h"
#include "dss/ui/display_view_model.h"
#include "dss/ui/log_view_model.h"
#include "dss/ui/network_view_model.h"
#include "dss/ui/processing_view_model.h"
#include "dss/ui/replay_view_model.h"
#include "dss/ui/serial_port_view_model.h"
#include "dss/ui/settings_view_model.h"
#include "dss/ui/storage_view_model.h"
#include "dss/ui/tracking_view_model.h"

namespace Dss::Ui {

/**
 * @brief UI 层主 ViewModel 与组合根。
 *
 * 该类负责创建所有业务子 ViewModel、汇总状态文本、连接跨模块关系，并响应主控事件。
 * 具体业务命令仍由对应子 ViewModel 承担，避免主 ViewModel 重新膨胀成单体对象。
 */
class MainViewModel : public QObject {
    Q_OBJECT

public:
    using MessageBus =
        Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock>;  ///< 事件总线类型别名。

    /**
     * @brief 构造 UI 层主 ViewModel。
     * @param bus 应用事件总线。
     * @param registry 应用服务注册表。
     * @param parent Qt 父对象。
     */
    explicit MainViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry,
                           QObject* parent = nullptr);

    /**
     * @brief 析构 UI 层主 ViewModel。
     */
    ~MainViewModel() override;

    /**
     * @brief 获取当前状态栏文本。
     * @return 最近一次子模块或协调逻辑发布的状态文本。
     */
    [[nodiscard]] auto statusText() const -> QString;

    /**
     * @brief 获取当前曝光时间。
     * @return 曝光时间，单位毫秒。
     */
    [[nodiscard]] double exposure() const;

    /** @brief 获取日志子 ViewModel。 @return 可写日志 ViewModel 引用。 */
    [[nodiscard]] LogViewModel& logs();
    /** @brief 获取只读日志子 ViewModel。 @return 日志 ViewModel 常量引用。 */
    [[nodiscard]] const LogViewModel& logs() const;
    /** @brief 获取回放子 ViewModel。 @return 可写回放 ViewModel 引用。 */
    [[nodiscard]] ReplayViewModel& replay();
    /** @brief 获取只读回放子 ViewModel。 @return 回放 ViewModel 常量引用。 */
    [[nodiscard]] const ReplayViewModel& replay() const;
    /** @brief 获取显示子 ViewModel。 @return 可写显示 ViewModel 引用。 */
    [[nodiscard]] DisplayViewModel& display();
    /** @brief 获取只读显示子 ViewModel。 @return 显示 ViewModel 常量引用。 */
    [[nodiscard]] const DisplayViewModel& display() const;
    /** @brief 获取处理子 ViewModel。 @return 可写处理 ViewModel 引用。 */
    [[nodiscard]] ProcessingViewModel& processing();
    /** @brief 获取只读处理子 ViewModel。 @return 处理 ViewModel 常量引用。 */
    [[nodiscard]] const ProcessingViewModel& processing() const;
    /** @brief 获取跟踪子 ViewModel。 @return 可写跟踪 ViewModel 引用。 */
    [[nodiscard]] TrackingViewModel& tracking();
    /** @brief 获取只读跟踪子 ViewModel。 @return 跟踪 ViewModel 常量引用。 */
    [[nodiscard]] const TrackingViewModel& tracking() const;
    /** @brief 获取存储子 ViewModel。 @return 可写存储 ViewModel 引用。 */
    [[nodiscard]] StorageViewModel& storage();
    /** @brief 获取只读存储子 ViewModel。 @return 存储 ViewModel 常量引用。 */
    [[nodiscard]] const StorageViewModel& storage() const;
    /** @brief 获取设置子 ViewModel。 @return 可写设置 ViewModel 引用。 */
    [[nodiscard]] SettingsViewModel& settings();
    /** @brief 获取只读设置子 ViewModel。 @return 设置 ViewModel 常量引用。 */
    [[nodiscard]] const SettingsViewModel& settings() const;
    /** @brief 获取串口子 ViewModel。 @return 可写串口 ViewModel 引用。 */
    [[nodiscard]] SerialPortViewModel& serialPorts();
    /** @brief 获取只读串口子 ViewModel。 @return 串口 ViewModel 常量引用。 */
    [[nodiscard]] const SerialPortViewModel& serialPorts() const;
    /** @brief 获取网络子 ViewModel。 @return 可写网络 ViewModel 引用。 */
    [[nodiscard]] NetworkViewModel& network();
    /** @brief 获取只读网络子 ViewModel。 @return 网络 ViewModel 常量引用。 */
    [[nodiscard]] const NetworkViewModel& network() const;
    /** @brief 获取数据交换子 ViewModel。 @return 可写数据交换 ViewModel 引用。 */
    [[nodiscard]] DataExchangeViewModel& dataExchange();
    /** @brief 获取只读数据交换子 ViewModel。 @return 数据交换 ViewModel 常量引用。 */
    [[nodiscard]] const DataExchangeViewModel& dataExchange() const;

public Q_SLOTS:
    /**
     * @brief 设置曝光时间。
     * @param ms 曝光时间，单位毫秒。
     */
    Q_INVOKABLE void setExposure(double ms);

Q_SIGNALS:
    /**
     * @brief 状态栏文本变化。
     * @param text 新的状态文本。
     */
    void statusTextChanged(const QString& text);

    /**
     * @brief 曝光时间变化。
     * @param ms 曝光时间，单位毫秒。
     */
    void exposureChanged(double ms);

private:
    /**
     * @brief 建立子模块之间以及子模块到主 ViewModel 的信号连接。
     */
    void connectChildViewModels();

    /**
     * @brief 订阅主 ViewModel 需要协调的核心事件。
     */
    void setupSubscriptions();

    /**
     * @brief 响应主控指令事件，协调曝光、跟踪、保存和回放状态。
     * @param event 主控事件。
     */
    void onMasterControl(const Dss::Core::MasterControlEvent& event);

    /**
     * @brief 更新状态文本并发出信号。
     * @param text 新的状态文本。
     */
    void setStatus(QString text);

    MessageBus& m_bus;                       ///< 应用事件总线。
    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    LogViewModel m_logs;                     ///< 日志子 ViewModel。
    ReplayViewModel m_replay;                ///< 回放子 ViewModel。
    DisplayViewModel m_display;              ///< 显示子 ViewModel。
    ProcessingViewModel m_processing;        ///< 处理子 ViewModel。
    TrackingViewModel m_tracking;            ///< 跟踪子 ViewModel。
    StorageViewModel m_storage;              ///< 存储子 ViewModel。
    SettingsViewModel m_settings;            ///< 设置子 ViewModel。
    SerialPortViewModel m_serialPorts;       ///< 串口子 ViewModel。
    NetworkViewModel m_network;              ///< 网络端点子 ViewModel。
    DataExchangeViewModel m_dataExchange;    ///< 数据交换子 ViewModel。
    QString m_statusText = "Ready";          ///< 最近一次状态栏文本。
    double m_exposure = 0.0;                 ///< 当前曝光时间，单位毫秒。
    std::string m_activeStorageSessionId;    ///< 当前主控存储会话标识。

    std::vector<Dss::Evt::ScopedConnection> m_connections;  ///< 事件订阅连接列表。
};

}  // namespace Dss::Ui
