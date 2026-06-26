#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace Dss::Storage {

/// 存储后端抽象接口，定义初始化与就绪状态查询
class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    /**
     * @brief 初始化存储后端并创建基础目录
     * @param baseDir 存储根目录路径
     * @return 成功时返回空值；失败时返回错误描述
     */
    virtual auto init(const std::filesystem::path& baseDir) -> std::expected<void, std::string> = 0;

    /**
     * @brief 查询存储后端是否已完成初始化。
     * @return 后端可接受存储请求时返回 true。
     */
    [[nodiscard]] virtual bool isReady() const = 0;
};

}  // namespace Dss::Storage
