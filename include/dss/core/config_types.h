#pragma once

#include <cstdint>
#include <string>

namespace Dss::Core {

/// 串口通信配置
struct SerialConfig {
    std::string portName;   ///< 端口名称
    int baudRate = 230400;  ///< 波特率
    int dataBits = 8;       ///< 数据位
    int stopBits = 1;       ///< 停止位
};

/// UDP 端点配置
struct UdpEndpointConfig {
    std::string localIp;      ///< 本地 IP 地址
    uint16_t localPort = 0;   ///< 本地端口
    std::string remoteIp;     ///< 远端 IP 地址
    uint16_t remotePort = 0;  ///< 远端端口
};

}  // namespace Dss::Core
