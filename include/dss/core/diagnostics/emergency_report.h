#pragma once

#include <cstdio>

namespace Dss::Core {

/// @brief 异常报告失效时向标准错误输出尽力写入诊断，保证异常不会继续传播。
/// @param[in] message 以空字符结尾的非空诊断文本。
/// @note 使用 C stdio；输出失败不再触发递归报告，不承诺消息持久化。
inline void reportEmergencyFailure(const char* message) noexcept {
    static_cast<void>(std::fputs(message, stderr));
}

}  // namespace Dss::Core
