#pragma once

#include "dss/comm/serial_protocol_codec.h"

namespace Dss::Comm {

/// @brief 曝光串口命令发送接口。
class IExposureCommandPort {
public:
    virtual ~IExposureCommandPort() = default;

    /**
     * @brief 缓存曝光命令并请求发送一帧。
     * @param command 曝光通道发送命令参数
     */
    virtual void sendExposureCommand(const ExposureCommand& command) = 0;
};

/// @brief 伺服修正命令发送接口。
class IServoCorrectionPort {
public:
    virtual ~IServoCorrectionPort() = default;

    /**
     * @brief 缓存伺服修正量并请求发送一帧。
     * @param correction 伺服距离/速度修正参数
     */
    virtual void sendServoCorrection(const ServoCorrection& correction) = 0;
};

/// @brief 主控状态回包发送接口。
class IMasterControlStatusPort {
public:
    virtual ~IMasterControlStatusPort() = default;

    /**
     * @brief 缓存主控状态并请求发送一帧。
     * @param status 主控状态回包内容
     */
    virtual void sendMasterControlStatus(const MasterControlStatus& status) = 0;
};

}  // namespace Dss::Comm
