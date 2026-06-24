#pragma once

#include <deque>
#include <span>
#include <vector>

#include "dss/tracking/i_tracking_strategy.h"

namespace Dss::Tracking {

/// 地球同步轨道恒星速度估算状态
enum class GeoStarSpeedStatus {
    Ok,                  ///< 估算成功
    InsufficientFrames,  ///< 帧数不足
    MissingStars,        ///< 缺少恒星测量
    NoCandidates,        ///< 无有效候选运动
};

/// 地球同步轨道恒星速度估算结果
struct GeoStarSpeedResult {
    GeoStarSpeedStatus status = GeoStarSpeedStatus::InsufficientFrames;  ///< 估算状态
    Dss::Core::Vec2f frameSpeed{};  ///< 像面坐标系下的恒星速度（像素/帧）
    Dss::Core::Vec2f aeSpeed{};     ///< 方位-俯仰坐标系下的恒星速度
    int matchCount = 0;             ///< 参与统计的恒星匹配对数
};

/**
 * @brief 根据连续帧恒星测量估算地球同步轨道背景恒星速度
 * @param frames 连续帧测量序列
 * @param ratioFov 视场中心区域比例，用于筛选可靠恒星
 * @param radius 恒星位移候选的最大搜索半径（像素）
 * @param opticParams 光学系统参数
 * @return 恒星速度估算结果
 */
[[nodiscard]] auto estimateGeoStarSpeed(std::span<const Dss::Core::FrameMeasurements> frames,
                                        float ratioFov, float radius,
                                        const Dss::Core::OpticParams& opticParams)
    -> GeoStarSpeedResult;

/// 地球同步轨道（GEO）目标跟踪器，基于四帧关联与恒星速度补偿
class GeoTracker final : public ITrackingStrategy {
public:
    /**
     * @brief 创建地球同步轨道目标跟踪器。
     * @param settings 跟踪门限、光学参数和生命周期配置。
     */
    explicit GeoTracker(const Dss::Core::TrackingSettings& settings);

    /**
     * @brief 处理单帧测量，执行恒星速度估算、目标关联与跟踪
     * @param measurements 当前帧测量数据
     * @return 当前活跃目标列表
     */
    auto track(const Dss::Core::FrameMeasurements& measurements)
        -> std::vector<Dss::Core::TargetInfo> override;

    /** @brief 获取策略模式。 @return 固定返回 TrackMode::Geo。 */
    [[nodiscard]] auto mode() const -> Dss::Core::TrackMode override {
        return Dss::Core::TrackMode::Geo;
    }

    /// 重置 FIFO 缓存、目标列表与恒星速度状态
    void reset() override;

private:
    /** @brief 估算当前帧间恒星背景速度。 @return 成功返回 0，否则返回 1。 */
    int calcStarSpeed();
    /** @brief 对最近四帧执行目标关联。 @return 生成候选时返回 0，否则返回 1。 */
    int assoc4();
    /** @brief 更新目标发现状态。 @return 找到目标时返回 0，否则返回 1。 */
    int findTargets();
    /** @brief 在跟踪过程中重新发现新目标。 @return 成功执行时返回 0，否则返回 1。 */
    int refindTargets();
    /** @brief 对已有目标执行逐帧跟踪。 @return 成功执行时返回 0，否则返回 1。 */
    int trackTargets();
    /**
     * @brief 为尚未分配 ID 的目标生成唯一标识。
     * @param targets 待检查并原位更新的目标列表。
     */
    void assignTargetIds(std::vector<Dss::Core::TargetInfo>& targets);

    Dss::Core::TrackingSettings m_settings;                 ///< 跟踪算法参数
    std::deque<Dss::Core::FrameMeasurements> m_fifoTarget;  ///< 目标测量帧 FIFO
    std::deque<Dss::Core::FrameMeasurements> m_fifoStar;    ///< 恒星测量帧 FIFO
    std::vector<Dss::Core::TargetInfo> m_targets;           ///< 当前跟踪的目标集合
    Dss::Core::Vec2f m_starSpeed{};                         ///< 像面恒星背景速度
    Dss::Core::Vec2f m_starSpeedAe{};                       ///< 方位-俯仰恒星背景速度
    float m_frameFreq = 1.0f;                               ///< 当前帧频率（Hz）
    int m_starMatchCount = 0;                               ///< 最近一次恒星速度匹配的恒星对数
    bool m_targetFound = false;                             ///< 是否已发现目标
    bool m_targetVerified = false;                          ///< 目标是否已通过验证
    uint64_t m_frameSeq = 0;                                ///< 最近处理的帧序号
    uint64_t m_nextTargetId = 1;                            ///< 下一个待分配的目标编号
};

}  // namespace Dss::Tracking
