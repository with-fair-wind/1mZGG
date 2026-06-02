#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dss/acquisition/camera_control_protocol.h"

namespace Dss::Acquisition {

class ICameraController {
public:
    virtual ~ICameraController() = default;

    [[nodiscard]] virtual bool isOpen() const = 0;
    [[nodiscard]] virtual auto portName() const -> std::string_view = 0;
    [[nodiscard]] virtual auto grabCommand(bool start) const -> CameraCommand = 0;
    [[nodiscard]] virtual auto exposureCommands(float exposureMilliseconds) const
        -> std::vector<CameraCommand> = 0;
    [[nodiscard]] virtual auto gainCommands(float gain) const -> std::vector<CameraCommand> = 0;
    [[nodiscard]] virtual auto windowCommands(int line, int start, int fullWidth,
                                              int subWidth) const -> std::vector<CameraCommand> = 0;
};

class CommandOnlyCameraController final : public ICameraController {
public:
    explicit CommandOnlyCameraController(std::string portName) : m_portName(std::move(portName)) {}

    [[nodiscard]] bool isOpen() const override {
        return false;
    }

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
    std::string m_portName;
};

}  // namespace Dss::Acquisition
