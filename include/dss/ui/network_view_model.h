#pragma once

#include <QObject>
#include <QString>
#include <vector>

#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief UI 层可编辑的 UDP 网络端点状态。
 */
struct NetworkEndpointState {
    QString key;           ///< 端点键名，用于回写配置。
    QString displayName;   ///< 界面显示名称。
    QString localIp;       ///< 本地 IP。
    int localPort = 0;     ///< 本地端口，0 表示自动分配。
    QString remoteIp;      ///< 远端 IP。
    int remotePort = 0;    ///< 远端端口。
    bool canOpen = false;  ///< 是否支持 UI 显式打开/关闭。
    bool isOpen = false;   ///< 对应服务当前是否已打开。
};

/**
 * @brief 网络端点子 ViewModel，负责普通 UDP 服务端点配置与显式开关。
 */
class NetworkViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造网络端点子 ViewModel。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit NetworkViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构网络端点子 ViewModel。
     */
    ~NetworkViewModel() override;

    /**
     * @brief 获取当前所有可编辑 UDP 网络端点。
     * @return 网络端点状态快照列表。
     */
    [[nodiscard]] auto networkEndpointConfigs() -> std::vector<NetworkEndpointState>;

    /**
     * @brief 查询指定网络端点对应服务是否已打开。
     * @param key 网络端点键名。
     * @return 服务已打开时返回 true。
     */
    [[nodiscard]] bool isNetworkEndpointOpen(const QString& key);

public Q_SLOTS:
    /**
     * @brief 应用单个 UDP 网络端点配置。
     * @param key 网络端点键名。
     * @param localIp 本地 IP。
     * @param localPort 本地端口，0 表示自动分配。
     * @param remoteIp 远端 IP。
     * @param remotePort 远端端口。
     * @return 参数合法并写入内存配置时返回 true。
     */
    Q_INVOKABLE bool applyNetworkEndpointConfig(const QString& key, const QString& localIp,
                                                int localPort, const QString& remoteIp,
                                                int remotePort);

    /**
     * @brief 显式打开指定网络端点对应的服务。
     * @param key 网络端点键名。
     * @return 成功打开或服务已打开时返回 true。
     */
    Q_INVOKABLE bool openNetworkEndpoint(const QString& key);

    /**
     * @brief 显式关闭指定网络端点对应的服务。
     * @param key 网络端点键名。
     */
    Q_INVOKABLE void closeNetworkEndpoint(const QString& key);

Q_SIGNALS:
    /**
     * @brief 所有网络端点配置变化。
     */
    void networkEndpointsChanged();

    /**
     * @brief 网络服务打开状态变化。
     * @param key 网络端点键名。
     * @param isOpen 是否已打开。
     */
    void networkServiceStateChanged(const QString& key, bool isOpen);

    /**
     * @brief 数据交换端点配置变化。
     */
    void dataExchangeEndpointsChanged();

    /**
     * @brief 数据交换端点被普通端点编辑器改写。
     */
    void dataExchangeEndpointEdited();

    /**
     * @brief 网络模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
};

}  // namespace Dss::Ui
