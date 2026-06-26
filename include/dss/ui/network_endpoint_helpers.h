#pragma once

#include <QString>

#include "dss/core/config_types.h"

namespace Dss::Ui {

/**
 * @brief 将 UDP 端点本地 IP 转换为 Qt 字符串。
 * @param endpoint UDP 端点配置。
 * @return 本地 IP 字符串。
 */
[[nodiscard]] auto endpointLocalIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString;

/**
 * @brief 将 UDP 端点远端 IP 转换为 Qt 字符串。
 * @param endpoint UDP 端点配置。
 * @return 远端 IP 字符串。
 */
[[nodiscard]] auto endpointRemoteIp(const Dss::Core::UdpEndpointConfig& endpoint) -> QString;

/**
 * @brief 判断本地 UDP 端口是否处于可配置范围。
 * @param port 端口号，0 表示由系统自动分配。
 * @return 端口合法时返回 true。
 */
[[nodiscard]] auto isLocalUdpPortInRange(int port) -> bool;

/**
 * @brief 判断远端 UDP 端口是否处于可配置范围。
 * @param port 端口号，必须大于 0。
 * @return 端口合法时返回 true。
 */
[[nodiscard]] auto isRemoteUdpPortInRange(int port) -> bool;

/**
 * @brief 根据 UI 输入构造规范化后的 UDP 端点配置。
 * @param localIp 本地 IP 输入。
 * @param localPort 本地端口。
 * @param remoteIp 远端 IP 输入。
 * @param remotePort 远端端口。
 * @return 去除首尾空白后的 UDP 端点配置。
 */
[[nodiscard]] auto makeUdpEndpointConfig(const QString& localIp, int localPort,
                                         const QString& remoteIp, int remotePort)
    -> Dss::Core::UdpEndpointConfig;

}  // namespace Dss::Ui
