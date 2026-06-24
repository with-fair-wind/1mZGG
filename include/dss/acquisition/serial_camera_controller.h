#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "dss/acquisition/i_camera_controller.h"

namespace Dss::Acquisition {

/// @brief 相机控制器所需的最小串口写入接口。
class ICameraSerialPort {
public:
    virtual ~ICameraSerialPort() = default;

    /** @brief 查询串口是否已打开。 @return 串口可写时返回 true。 */
    [[nodiscard]] virtual bool isOpen() const = 0;

    /** @brief 获取串口名称。 @return 串口名称的非拥有字符串视图。 */
    [[nodiscard]] virtual auto portName() const -> std::string_view = 0;

    /**
     * @brief 向相机串口写入完整指令。
     * @param bytes 待发送字节。
     * @return 写入成功时为空；失败时返回错误描述。
     */
    virtual auto write(std::span<const uint8_t> bytes) -> std::expected<void, std::string> = 0;
};

/// @brief 生成相机控制指令并通过串口顺序发送的控制器。
class SerialCameraController final : public ICameraController {
public:
    /**
     * @brief 创建串口相机控制器。
     * @param port 串口共享实例；空指针表示控制器不可发送。
     */
    explicit SerialCameraController(std::shared_ptr<ICameraSerialPort> port);

    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] auto portName() const -> std::string_view override;
    [[nodiscard]] auto grabCommand(bool start) const -> CameraCommand override;
    [[nodiscard]] auto exposureCommands(float exposureMilliseconds) const
        -> std::vector<CameraCommand> override;
    [[nodiscard]] auto gainCommands(float gain) const -> std::vector<CameraCommand> override;
    [[nodiscard]] auto windowCommands(int line, int start, int fullWidth, int subWidth) const
        -> std::vector<CameraCommand> override;

    /**
     * @brief 发送采集启停指令。
     * @param start true 表示开始采集，false 表示停止采集。
     * @return 发送成功时为空；串口不可用或写入失败时返回错误描述。
     */
    auto sendGrab(bool start) -> std::expected<void, std::string>;

    /**
     * @brief 发送曝光时间指令序列。
     * @param exposureMilliseconds 曝光时间，单位为毫秒。
     * @return 全部指令发送成功时为空，否则返回首个错误。
     */
    auto sendExposure(float exposureMilliseconds) -> std::expected<void, std::string>;

    /**
     * @brief 发送增益指令序列。
     * @param gain 相机增益值。
     * @return 全部指令发送成功时为空，否则返回首个错误。
     */
    auto sendGain(float gain) -> std::expected<void, std::string>;

    /**
     * @brief 发送 ROI 窗口指令序列。
     * @param line 窗口行数。
     * @param start 起始行偏移。
     * @param fullWidth 全幅行数。
     * @param subWidth 子窗口行数。
     * @return 参数及发送均成功时为空，否则返回错误描述。
     */
    auto sendWindow(int line, int start, int fullWidth, int subWidth)
        -> std::expected<void, std::string>;

private:
    /**
     * @brief 按顺序发送相机指令。
     * @param commands 待发送指令序列。
     * @return 全部写入成功时为空，否则返回首个错误。
     */
    auto sendCommands(std::span<const CameraCommand> commands) -> std::expected<void, std::string>;

    std::shared_ptr<ICameraSerialPort> m_port;  ///< 相机串口共享实例
};

}  // namespace Dss::Acquisition
