#pragma once

#include <QObject>
#include <QString>

#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 数据交换子 ViewModel，负责 GXTC/GDCL UDP 端点与显式开关。
 */
class DataExchangeViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造数据交换子 ViewModel。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit DataExchangeViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构数据交换子 ViewModel。
     */
    ~DataExchangeViewModel() override;

    /**
     * @brief 查询数据交换 UDP 是否已显式打开。
     * @return 两路数据交换 UDP 均打开时返回 true。
     */
    [[nodiscard]] bool isDataExchangeOpen() const;

    /// GXTC 数据交换本地 IP。
    [[nodiscard]] QString dataExchangeGxtcLocalIp() const;
    /// GXTC 数据交换本地端口。
    [[nodiscard]] int dataExchangeGxtcLocalPort() const;
    /// GXTC 数据交换远端 IP。
    [[nodiscard]] QString dataExchangeGxtcRemoteIp() const;
    /// GXTC 数据交换远端端口。
    [[nodiscard]] int dataExchangeGxtcRemotePort() const;
    /// GDCL 数据交换本地 IP。
    [[nodiscard]] QString dataExchangeGdclLocalIp() const;
    /// GDCL 数据交换本地端口。
    [[nodiscard]] int dataExchangeGdclLocalPort() const;
    /// GDCL 数据交换远端 IP。
    [[nodiscard]] QString dataExchangeGdclRemoteIp() const;
    /// GDCL 数据交换远端端口。
    [[nodiscard]] int dataExchangeGdclRemotePort() const;

    /**
     * @brief 因外部端点重配关闭数据交换服务。
     *
     * 该方法不更新状态栏文本，供普通网络端点编辑器重配 exchange_gxtc/exchange_gdcl 时使用。
     */
    void closeForEndpointReconfigure();

public Q_SLOTS:
    /**
     * @brief 应用 GXTC/GDCL 数据交换端点配置。
     * @return 参数合法并写入内存配置时返回 true。
     */
    Q_INVOKABLE bool applyDataExchangeEndpoints(const QString& gxtcLocalIp, int gxtcLocalPort,
                                                const QString& gxtcRemoteIp, int gxtcRemotePort,
                                                const QString& gdclLocalIp, int gdclLocalPort,
                                                const QString& gdclRemoteIp, int gdclRemotePort);

    /**
     * @brief 显式打开 GXTC/GDCL 数据交换 UDP 通道。
     * @return 成功打开时返回 true。
     */
    Q_INVOKABLE bool openDataExchange();

    /**
     * @brief 显式关闭 GXTC/GDCL 数据交换 UDP 通道。
     */
    Q_INVOKABLE void closeDataExchange();

Q_SIGNALS:
    /**
     * @brief 数据交换 UDP 打开状态变化。
     * @param value 是否已打开。
     */
    void dataExchangeOpenChanged(bool value);

    /**
     * @brief 数据交换端点配置变化。
     */
    void dataExchangeEndpointsChanged();

    /**
     * @brief 数据交换模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 设置数据交换 UDP 打开状态并发出信号。
     * @param value 是否已打开。
     */
    void setDataExchangeOpen(bool value);

    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    bool m_dataExchangeOpen = false;         ///< 数据交换 UDP 是否已显式打开。
};

}  // namespace Dss::Ui
