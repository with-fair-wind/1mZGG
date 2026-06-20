#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "dss/acquisition/i_camera_controller.h"

namespace Dss::Acquisition {

class ICameraSerialPort {
public:
    virtual ~ICameraSerialPort() = default;
    [[nodiscard]] virtual bool isOpen() const = 0;
    [[nodiscard]] virtual auto portName() const -> std::string_view = 0;
    virtual auto write(std::span<const uint8_t> bytes) -> std::expected<void, std::string> = 0;
};

class SerialCameraController final : public ICameraController {
public:
    explicit SerialCameraController(std::shared_ptr<ICameraSerialPort> port);

    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] auto portName() const -> std::string_view override;
    [[nodiscard]] auto grabCommand(bool start) const -> CameraCommand override;
    [[nodiscard]] auto exposureCommands(float exposureMilliseconds) const
        -> std::vector<CameraCommand> override;
    [[nodiscard]] auto gainCommands(float gain) const -> std::vector<CameraCommand> override;
    [[nodiscard]] auto windowCommands(int line, int start, int fullWidth, int subWidth) const
        -> std::vector<CameraCommand> override;

    auto sendGrab(bool start) -> std::expected<void, std::string>;
    auto sendExposure(float exposureMilliseconds) -> std::expected<void, std::string>;
    auto sendGain(float gain) -> std::expected<void, std::string>;
    auto sendWindow(int line, int start, int fullWidth, int subWidth)
        -> std::expected<void, std::string>;

private:
    auto sendCommands(std::span<const CameraCommand> commands) -> std::expected<void, std::string>;

    std::shared_ptr<ICameraSerialPort> m_port;
};

}  // namespace Dss::Acquisition
