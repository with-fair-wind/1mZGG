#include "dss/acquisition/serial_camera_controller.h"

#include <array>
#include <utility>

namespace Dss::Acquisition {

SerialCameraController::SerialCameraController(std::shared_ptr<ICameraSerialPort> port)
    : m_port(std::move(port)) {}

bool SerialCameraController::isOpen() const {
    return m_port && m_port->isOpen();
}

auto SerialCameraController::portName() const -> std::string_view {
    return m_port ? m_port->portName() : std::string_view{};
}

auto SerialCameraController::grabCommand(bool start) const -> CameraCommand {
    return buildGrabCommand(start);
}

auto SerialCameraController::exposureCommands(float exposureMilliseconds) const
    -> std::vector<CameraCommand> {
    return buildExposureCommands(exposureMilliseconds);
}

auto SerialCameraController::gainCommands(float gain) const -> std::vector<CameraCommand> {
    return buildGainCommands(gain);
}

auto SerialCameraController::windowCommands(int line, int start, int fullWidth, int subWidth) const
    -> std::vector<CameraCommand> {
    return buildWindowCommands(line, start, fullWidth, subWidth);
}

auto SerialCameraController::sendGrab(bool start) -> std::expected<void, std::string> {
    const std::array<CameraCommand, 1> commands{grabCommand(start)};
    return sendCommands(commands);
}

auto SerialCameraController::sendExposure(float exposureMilliseconds)
    -> std::expected<void, std::string> {
    const auto commands = exposureCommands(exposureMilliseconds);
    return sendCommands(commands);
}

auto SerialCameraController::sendGain(float gain) -> std::expected<void, std::string> {
    const auto commands = gainCommands(gain);
    return sendCommands(commands);
}

auto SerialCameraController::sendWindow(int line, int start, int fullWidth, int subWidth)
    -> std::expected<void, std::string> {
    const auto commands = windowCommands(line, start, fullWidth, subWidth);
    if (commands.empty()) {
        return std::unexpected("camera window parameters are invalid");
    }
    return sendCommands(commands);
}

auto SerialCameraController::sendCommands(std::span<const CameraCommand> commands)
    -> std::expected<void, std::string> {
    if (!m_port || !m_port->isOpen()) {
        return std::unexpected("camera serial port is not open");
    }
    for (const auto& command : commands) {
        const auto result = m_port->write(command);
        if (!result.has_value()) {
            return result;
        }
    }
    return {};
}

}  // namespace Dss::Acquisition
