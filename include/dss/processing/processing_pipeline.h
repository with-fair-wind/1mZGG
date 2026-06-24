#pragma once

#include <memory>
#include <string_view>

#include "dss/processing/i_processing_strategy.h"

namespace Dss::Processing {

/// 处理管线，封装可替换的处理策略后端
class ProcessingPipeline {
public:
    /**
     * @brief 设置处理策略后端
     * @param backend 处理策略实例的所有权
     */
    void setBackend(std::unique_ptr<IProcessingStrategy> backend);

    /**
     * @brief 处理单帧图像
     * @param packet 输入帧数据包
     * @return 处理结果；无后端时返回空结果
     */
    [[nodiscard]] auto process(const FramePacket& packet) -> ProcessingResult;

    /**
     * @brief 获取当前处理模式。
     * @return 后端模式；无后端时返回 ProcessingMode::None。
     */
    [[nodiscard]] auto currentMode() const -> Dss::Core::ProcessingMode;

    /**
     * @brief 获取当前后端名称。
     * @return 后端名称；无后端时返回 "none"。
     */
    [[nodiscard]] auto backendName() const -> std::string_view;

    /** @brief 查询是否已设置后端。 @return 存在处理后端时返回 true。 */
    [[nodiscard]] bool hasBackend() const;

private:
    std::unique_ptr<IProcessingStrategy> m_backend;  ///< 当前处理策略后端
};

}  // namespace Dss::Processing
