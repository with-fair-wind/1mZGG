#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "dss/core/constants.h"

namespace Dss::Comm {

/// @brief 固定帧校验失败原因。
enum class FrameValidationFailure {
    None,            ///< 未失败，帧合法
    SizeMismatch,    ///< 实际帧长与期望帧长不一致
    TooShort,        ///< 帧长度不足以包含头尾字节
    HeaderMismatch,  ///< 帧头字节不匹配
    TailMismatch,    ///< 帧尾字节不匹配
};

/// @brief 固定帧校验的详细结果。
struct FrameValidationResult {
    bool valid = false;                                             ///< 帧是否合法
    FrameValidationFailure failure = FrameValidationFailure::None;  ///< 校验失败原因
    std::size_t expectedSize = 0;                                   ///< 期望帧长
    std::size_t actualSize = 0;                                     ///< 实际帧长
    uint8_t observedHeader = 0;                                     ///< 实际帧头字节
    uint8_t observedTail = 0;                                       ///< 实际帧尾字节
};

/// 帧头尾校验与封装工具
class FrameCodec {
public:
    static constexpr uint8_t HEADER = Dss::Core::FrameHeader;  ///< 帧头标识字节
    static constexpr uint8_t TAIL = Dss::Core::FrameTail;      ///< 帧尾标识字节

    /**
     * @brief 校验帧长度及头尾标识并返回详细诊断。
     * @param data 待校验的帧数据
     * @param expectedSize 期望的帧长度
     * @return 包含合法标记、失败原因和观测字节的校验结果
     */
    [[nodiscard]] static auto validateDetailed(std::span<const uint8_t> data,
                                               std::size_t expectedSize) -> FrameValidationResult {
        FrameValidationResult result{
            .expectedSize = expectedSize,
            .actualSize = data.size(),
        };
        if (!data.empty()) {
            result.observedHeader = data.front();
            result.observedTail = data.back();
        }

        if (data.size() != expectedSize) {
            result.failure = FrameValidationFailure::SizeMismatch;
            return result;
        }
        if (data.size() < 2U) {
            result.failure = FrameValidationFailure::TooShort;
            return result;
        }
        if (data.front() != HEADER) {
            result.failure = FrameValidationFailure::HeaderMismatch;
            return result;
        }
        if (data.back() != TAIL) {
            result.failure = FrameValidationFailure::TailMismatch;
            return result;
        }

        result.valid = true;
        return result;
    }

    /**
     * @brief 校验帧长度及头尾标识
     * @param data 待校验的帧数据
     * @param expectedSize 期望的帧长度
     * @return 长度与头尾均匹配时返回 true
     */
    [[nodiscard]] static bool validate(std::span<const uint8_t> data, std::size_t expectedSize) {
        return validateDetailed(data, expectedSize).valid;
    }

    /**
     * @brief 将校验失败原因转换为可展示文本。
     * @param failure 校验失败原因
     * @return 稳定的英文诊断文本
     */
    [[nodiscard]] static constexpr auto failureMessage(FrameValidationFailure failure)
        -> std::string_view {
        switch (failure) {
            case FrameValidationFailure::None:
                return "serial frame is valid";
            case FrameValidationFailure::SizeMismatch:
                return "serial frame size mismatch";
            case FrameValidationFailure::TooShort:
                return "serial frame is too short";
            case FrameValidationFailure::HeaderMismatch:
                return "serial frame header mismatch";
            case FrameValidationFailure::TailMismatch:
                return "serial frame tail mismatch";
        }
        return "serial frame validation failed";
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
