#pragma once

#include <cstdint>
#include <string>

namespace Dss::Core {

struct SerialConfig {
    std::string portName;
    int baudRate = 230400;
    int dataBits = 8;
    int stopBits = 1;
};

struct UdpEndpointConfig {
    std::string localIp;
    uint16_t localPort = 0;
    std::string remoteIp;
    uint16_t remotePort = 0;
};

}  // namespace Dss::Core
