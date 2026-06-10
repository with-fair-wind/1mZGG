#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace Dss::Gpu {

/// GPU 后端抽象接口，统一设备初始化与状态查询。
class IGpuBackend {
public:
    virtual ~IGpuBackend() = default;

    /**
     * @brief 初始化 GPU 后端
     * @return 成功时返回空值，失败时返回错误描述
     */
    virtual auto init() -> std::expected<void, std::string> = 0;

    /// 是否已完成初始化
    [[nodiscard]] virtual bool isInitialized() const = 0;

    /// 获取当前 GPU 设备名称
    [[nodiscard]] virtual auto deviceName() const -> std::string = 0;
};

}  // namespace Dss::Gpu
