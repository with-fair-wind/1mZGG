#pragma once

#include "dss/comm/frame_codec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Dss::Comm
{

enum class SerialProtocol
{
    Display,
    Exposure,
    MasterControl,
    Servo,
};

struct SerialFrameLayout
{
    SerialProtocol protocol;
    std::string_view name;
    std::size_t recvSize = 0;
    std::size_t sendSize = 0;
};

inline constexpr std::array<SerialFrameLayout, 4> SerialFrameLayouts{
    SerialFrameLayout{SerialProtocol::Display, "display", 20, 9},
    SerialFrameLayout{SerialProtocol::Exposure, "exposure", 23, 8},
    SerialFrameLayout{SerialProtocol::MasterControl, "master_control", 30, 28},
    SerialFrameLayout{SerialProtocol::Servo, "servo", 20, 14},
};

[[nodiscard]] constexpr auto layoutFor(SerialProtocol protocol) -> SerialFrameLayout
{
    for (const auto& layout : SerialFrameLayouts)
    {
        if (layout.protocol == protocol)
        {
            return layout;
        }
    }
    return {};
}

[[nodiscard]] inline bool validateReceiveFrame(
    SerialProtocol protocol,
    std::span<const uint8_t> frame)
{
    return FrameCodec::validate(frame, layoutFor(protocol).recvSize);
}

inline bool prepareSendFrame(SerialProtocol protocol, std::span<uint8_t> frame)
{
    if (frame.size() != layoutFor(protocol).sendSize)
    {
        return false;
    }
    FrameCodec::wrap(frame);
    return true;
}

} // namespace Dss::Comm
