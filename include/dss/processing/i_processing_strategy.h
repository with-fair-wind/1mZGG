#pragma once

#include <string_view>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"
#include "dss/processing/frame_packet.h"

namespace Dss::Processing {

/// 单帧图像处理结果
struct ProcessingResult {
    Dss::Core::ImageStats stats{};                              ///< 图像统计信息
    std::vector<Dss::Core::MeasuredBlob> targetBlobs;           ///< 检测到的目标光斑
    std::vector<Dss::Core::MeasuredBlob> validatedTargetBlobs; ///< 校验通过的目标光斑
    std::vector<Dss::Core::MeasuredBlob> starBlobs;           ///< 检测到的恒星光斑
    std::vector<uint8_t> displayImage;                           ///< 8 位显示用图像
    bool success = false;                                       ///< 处理是否成功
};

/// 图像处理策略接口，定义可插拔的处理后端
class IProcessingStrategy {
public:
    virtual ~IProcessingStrategy() = default;

    /**
     * @brief 处理单帧图像
     * @param input 输入帧数据包
     * @return 处理结果
     */
    [[nodiscard]] virtual auto process(const FramePacket& input) -> ProcessingResult = 0;

    /// 策略名称标识
    [[nodiscard]] virtual auto name() const -> std::string_view = 0;

    /// 策略对应的处理模式
    [[nodiscard]] virtual auto mode() const -> Dss::Core::ProcessingMode = 0;
};

}  // namespace Dss::Processing
