#pragma once

#include <cstdint>
#include <span>

#include "dss/core/constants.h"

namespace Dss::Comm {

/// 帧头尾校验与封装工具
class FrameCodec {
public:
    static constexpr uint8_t HEADER = Dss::Core::FrameHeader;  ///< 帧头标识字节
    static constexpr uint8_t TAIL = Dss::Core::FrameTail;        ///< 帧尾标识字节

    /**
     * @brief 校验帧长度及头尾标识
     * @param data 待校验的帧数据
     * @param expectedSize 期望的帧长度
     * @return 长度与头尾均匹配时返回 true
     */
    [[nodiscard]] static bool validate(std::span<const uint8_t> data, size_t expectedSize) {
        if (data.size() != expectedSize) {
            return false;
        }
        if (data.size() < 2U) {
            return false;
        }
        return data.front() == HEADER && data.back() == TAIL;
    }

    /**
     * @brief 在缓冲区首尾写入帧头与帧尾
     * @param buffer 待封装的帧缓冲区
     */
    static void wrap(std::span<uint8_t> buffer) {
        if (!buffer.empty()) {
            buffer.front() = HEADER;
            buffer.back() = TAIL;
        }
    }
};

}  // namespace Dss::Comm
