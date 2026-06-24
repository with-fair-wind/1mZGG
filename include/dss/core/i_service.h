#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace Dss::Core {

/// 可启动/停止的后台服务接口
class IService {
public:
    virtual ~IService() = default;

    /**
     * @brief 获取服务名称。
     * @return 用于诊断和日志输出的稳定名称。
     */
    [[nodiscard]] virtual auto name() const -> std::string_view = 0;

    /**
     * @brief 启动服务
     * @return 失败时返回包含错误信息的 std::expected
     */
    virtual auto start() -> std::expected<void, std::string> = 0;

    /// 停止服务（不抛出异常）
    virtual void stop() noexcept = 0;
};

}  // namespace Dss::Core
