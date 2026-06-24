#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dss/acquisition/camera_control_protocol.h"

namespace Dss::Acquisition {

/// 相机控制器接口，负责生成相机寄存器控制指令
class ICameraController {
public:
    virtual ~ICameraController() = default;

    /** @brief 查询串口是否已打开。 @return 串口可用时返回 true。 */
    [[nodiscard]] virtual bool isOpen() const = 0;

    /** @brief 获取当前绑定的串口名称。 @return 串口名称的非拥有字符串视图。 */
    [[nodiscard]] virtual auto portName() const -> std::string_view = 0;

    /**
     * @brief 生成采集启停指令
     * @param start true 表示开始采集，false 表示停止采集
     * @return 3 字节采集控制指令
     */
    [[nodiscard]] virtual auto grabCommand(bool start) const -> CameraCommand = 0;

    /**
     * @brief 生成曝光时间寄存器指令序列
     * @param exposureMilliseconds 曝光时间（毫秒）
     * @return 寄存器写入指令向量
     */
    [[nodiscard]] virtual auto exposureCommands(float exposureMilliseconds) const
        -> std::vector<CameraCommand> = 0;

    /**
     * @brief 生成增益寄存器指令序列
     * @param gain 增益值
     * @return 寄存器写入指令向量
     */
    [[nodiscard]] virtual auto gainCommands(float gain) const -> std::vector<CameraCommand> = 0;

    /**
     * @brief 生成 ROI 窗口寄存器指令序列
     * @param line 行数（全幅或子窗口高度）
     * @param start 起始行偏移
     * @param fullWidth 全幅行数
     * @param subWidth 子窗口行数
     * @return 寄存器写入指令向量
     */
    [[nodiscard]] virtual auto windowCommands(int line, int start, int fullWidth,
                                              int subWidth) const -> std::vector<CameraCommand> = 0;
};

/// 仅生成控制指令、不持有真实串口连接的相机控制器实现
class CommandOnlyCameraController final : public ICameraController {
public:
    /**
     * @brief 构造指令型相机控制器
     * @param portName 逻辑串口名称（用于标识，不实际打开）
     */
    explicit CommandOnlyCameraController(std::string portName) : m_portName(std::move(portName)) {}

    /** @brief 查询串口状态。 @return 始终返回 false，因为该实现不持有真实串口。 */
    [[nodiscard]] bool isOpen() const override {
        return false;
    }

    /** @brief 获取逻辑串口名称。 @return 构造时指定的串口名称。 */
    [[nodiscard]] auto portName() const -> std::string_view override {
        return m_portName;
    }

    [[nodiscard]] auto grabCommand(bool start) const -> CameraCommand override {
        return buildGrabCommand(start);
    }

    [[nodiscard]] auto exposureCommands(float exposureMilliseconds) const
        -> std::vector<CameraCommand> override {
        return buildExposureCommands(exposureMilliseconds);
    }

    [[nodiscard]] auto gainCommands(float gain) const -> std::vector<CameraCommand> override {
        return buildGainCommands(gain);
    }

    [[nodiscard]] auto windowCommands(int line, int start, int fullWidth, int subWidth) const
        -> std::vector<CameraCommand> override {
        return buildWindowCommands(line, start, fullWidth, subWidth);
    }

private:
    std::string m_portName;  ///< 逻辑串口名称
};

}  // namespace Dss::Acquisition
