#pragma once

#include <cstdint>
#include <span>

#include "dss/core/constants.h"

namespace Dss::Comm {

class FrameCodec {
public:
    static constexpr uint8_t HEADER = Dss::Core::FrameHeader;
    static constexpr uint8_t TAIL = Dss::Core::FrameTail;

    [[nodiscard]] static bool validate(std::span<const uint8_t> data, size_t expectedSize) {
        if (data.size() != expectedSize) {
            return false;
        }
        if (data.size() < 2U) {
            return false;
        }
        return data.front() == HEADER && data.back() == TAIL;
    }

    static void wrap(std::span<uint8_t> buffer) {
        if (!buffer.empty()) {
            buffer.front() = HEADER;
            buffer.back() = TAIL;
        }
    }
};

}  // namespace Dss::Comm
