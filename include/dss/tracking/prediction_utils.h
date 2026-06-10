#pragma once

#include <cstddef>

#include "dss/core/types.h"

namespace Dss::Tracking {

/// 像斑匹配所用的坐标空间
enum class BlobMatchSpace {
    Frame,  ///< 像面像素坐标
    Ae,     ///< 方位-俯仰坐标
};

/// 像斑最近邻匹配选项
struct BlobMatchOptions {
    BlobMatchSpace space = BlobMatchSpace::Frame;  ///< 匹配坐标空间
    float threshold = 0.0F;                        ///< 各轴最大允许偏差
    bool requireFovCenter = false;                 ///< 是否要求候选像斑位于视场中心区域
    float fovCenterHalfExtent = 128.0F;            ///< 视场中心区域的半宽（像素）
};

/// 计算三个浮点数的中位数
[[nodiscard]] auto median3(float first, float second, float third) -> float;

/**
 * @brief 由帧测量与像斑构造有效的目标帧信息
 * @param frame 帧测量数据
 * @param blob 关联的像斑
 * @param settings 跟踪参数
 * @return 标记为有效的 TargetFrameInfo
 */
[[nodiscard]] auto makeTargetFrameInfo(const Core::FrameMeasurements& frame,
                                       const Core::MeasuredBlob& blob,
                                       const Core::TrackingSettings& settings)
    -> Core::TargetFrameInfo;

/**
 * @brief 构造未匹配到像斑时的占位帧信息（基于预测位置）
 * @param frame 帧测量数据
 * @param target 当前目标状态
 * @param settings 跟踪参数
 * @param halfExtent 占位像斑的半宽（像素）
 * @return 标记为无效的 TargetFrameInfo
 */
[[nodiscard]] auto makeInvalidTargetFrameInfo(const Core::FrameMeasurements& frame,
                                              const Core::TargetInfo& target,
                                              const Core::TrackingSettings& settings,
                                              float halfExtent = 5.0F) -> Core::TargetFrameInfo;

/// 由帧测量计算帧周期（秒）
[[nodiscard]] auto framePeriodSeconds(const Core::FrameMeasurements& frame) -> float;

/// 由目标帧信息计算帧周期（秒）
[[nodiscard]] auto framePeriodSeconds(const Core::TargetFrameInfo& frame) -> float;

/// 计算两像斑在像面坐标系下的位移
[[nodiscard]] auto frameMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to)
    -> Core::Vec2f;

/// 计算两像斑在方位-俯仰坐标系下的位移
[[nodiscard]] auto aeMotion(const Core::MeasuredBlob& from, const Core::MeasuredBlob& to)
    -> Core::Vec2f;

/**
 * @brief 计算目标在指定帧索引处的像面位移
 * @param target 目标轨迹
 * @param index 帧索引（须 ≥ 1）
 * @return 像面位移向量
 */
[[nodiscard]] auto targetFrameMotionAt(const Core::TargetInfo& target, std::size_t index)
    -> Core::Vec2f;

/**
 * @brief 计算目标在指定帧索引处的方位-俯仰位移
 * @param target 目标轨迹
 * @param index 帧索引（须 ≥ 1）
 * @return 方位-俯仰位移向量
 */
[[nodiscard]] auto targetAeMotionAt(const Core::TargetInfo& target, std::size_t index)
    -> Core::Vec2f;

/**
 * @brief 判断像斑是否位于视场中心附近
 * @param blob 待检查的像斑
 * @param settings 跟踪参数（含光学中心）
 * @param halfExtent 中心区域半宽（像素）
 * @return 在中心区域内返回 true
 */
[[nodiscard]] bool isNearFovCenter(const Core::MeasuredBlob& blob,
                                   const Core::TrackingSettings& settings, float halfExtent);

/**
 * @brief 在当前帧中查找与目标预测位置最近的像斑
 * @param frame 当前帧测量
 * @param target 目标状态（含预测位置）
 * @param settings 跟踪参数
 * @param options 匹配选项（坐标空间、阈值等）
 * @return 最近像斑指针；未找到时返回 nullptr
 */
[[nodiscard]] auto findNearestBlob(const Core::FrameMeasurements& frame,
                                   const Core::TargetInfo& target,
                                   const Core::TrackingSettings& settings,
                                   const BlobMatchOptions& options) -> const Core::MeasuredBlob*;

/// 根据最近四帧运动的中位数更新目标预测位置与速度
void updatePredictionFromRecentFour(Core::TargetInfo& target);

}  // namespace Dss::Tracking
