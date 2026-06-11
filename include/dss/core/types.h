#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Dss::Core {

/// 时间戳，精确到微秒
struct Timestamp {
    int year = 0;         ///< 年
    int month = 0;        ///< 月
    int day = 0;          ///< 日
    int hour = 0;         ///< 时
    int minute = 0;       ///< 分
    int second = 0;       ///< 秒
    int millisecond = 0;  ///< 毫秒
    int microsecond = 0;  ///< 微秒
};

/// 日内时刻（时:分:秒）
struct TimeOfDay {
    int hour = 0;    ///< 时
    int minute = 0;  ///< 分
    int second = 0;  ///< 秒
};

/// 光学参数配置
struct OpticParams {
    int imageWidth = 6144;           ///< 图像宽度（像素）
    int imageHeight = 6144;          ///< 图像高度（像素）
    float fovCenterX = 6144 / 2.0f;  ///< 视场中心 X 坐标（像素）
    float fovCenterY = 6144 / 2.0f;  ///< 视场中心 Y 坐标（像素）
    float pixelScale = 0.0003453f;   ///< 像素角尺度（度/像素）
};

/// 二维浮点向量
struct Vec2f {
    float x = 0.0f;  ///< X 分量
    float y = 0.0f;  ///< Y 分量
};

/// 二维双精度向量
struct Vec2d {
    double x = 0.0;  ///< X 分量
    double y = 0.0;  ///< Y 分量
};

/// 单个光斑检测结果
struct MeasuredBlob {
    std::string id;     ///< 目标标识
    Vec2f centroid{};   ///< 质心坐标（像素）
    float maxX = 0.0f;  ///< 包围盒最大 X（像素）
    float minX = 0.0f;  ///< 包围盒最小 X（像素）
    float maxY = 0.0f;  ///< 包围盒最大 Y（像素）
    float minY = 0.0f;  ///< 包围盒最小 Y（像素）
    float dn = 0.0f;    ///< 灰度值总和
    float area = 0.0f;  ///< 面积（像素²）
    Vec2f posAe{};      ///< 方位角/俯仰角位置（度）

    float fovCenterAziModify = 0.0f;  ///< 修正后的视场中心方位角（度）
    float fovCenterEleModify = 0.0f;  ///< 修正后的视场中心俯仰角（度）
    float distAzi = 0.0f;             ///< 方位角偏差（度）
    float distEle = 0.0f;             ///< 俯仰角偏差（度）
    float targetAzi = 0.0f;           ///< 目标方位角（度）
    float targetEle = 0.0f;           ///< 目标俯仰角（度）
    double pointErrEle = 0.0;         ///< 指向误差俯仰分量

    double alpha = 0.0;  ///< 赤道坐标 α（弧度，赤经分量）
    double sigma = 0.0;  ///< 赤道坐标 σ（弧度，赤纬分量）
    double ra = 0.0;     ///< 赤经（度）
    double dec = 0.0;    ///< 赤纬（度）
};

/// 单帧全部检测结果
struct FrameMeasurements {
    Timestamp timestamp{};                           ///< 帧时间戳
    uint64_t frameSeq = 0;                           ///< 帧序号
    Vec2f fovCenterAe{};                             ///< 视场中心方位/俯仰（度）
    float exposureTime = 0.0f;                       ///< 曝光时间（秒）
    float frameFreq = 0.0f;                          ///< 帧频（Hz）
    std::vector<MeasuredBlob> targetBlobs;           ///< 检测到的目标光斑
    std::vector<MeasuredBlob> validatedTargetBlobs;  ///< 已验证的目标光斑
    std::vector<MeasuredBlob> starBlobs;             ///< 恒星参考光斑
    double temperature = 0.0;                        ///< 环境温度
    double atmosPressure = 0.0;                      ///< 大气压
};

/// 单帧目标状态
struct TargetFrameInfo {
    Timestamp timestamp{};        ///< 帧时间戳
    uint64_t frameSeq = 0;        ///< 帧序号
    Vec2f fovCenterAe{};          ///< 视场中心方位/俯仰（度）
    Vec2f opticCenter{};          ///< 光学中心（像素）
    float exposureTime = 0.0f;    ///< 曝光时间（秒）
    float frameFreq = 0.0f;       ///< 帧频（Hz）
    MeasuredBlob measuredBlob{};  ///< 测量光斑
    Vec2f posZxdw{};              ///< 轴系定位坐标
    Vec2f posTwdw{};              ///< 天文定位坐标
    float magnitude = 0.0f;       ///< 星等
    bool valid = false;           ///< 本帧是否有效
};

/// 跨帧目标跟踪状态
struct TargetInfo {
    std::string targetId;                     ///< 目标标识
    std::string saveStartTime;                ///< 数据保存起始时间
    std::string filenameGae;                  ///< GAE 文件名
    std::vector<TargetFrameInfo> frameInfos;  ///< 各帧目标信息
    Vec2f predictedPosFrame{};                ///< 预测位置（像素）
    Vec2f predictedPosAe{};                   ///< 预测位置（度）
    Vec2f predictedSpdFrame{};                ///< 预测速度（像素/帧）
    Vec2f predictedSpdAe{};                   ///< 预测角速度（度/秒）
    float validity = 1.0f;                    ///< 有效性评分（0~1）
    bool living = false;                      ///< 是否处于活跃跟踪状态
    Vec2f lastRmDm{};                         ///< 最近一次赤经/赤纬修正量
};

/// 图像统计量
struct ImageStats {
    double maxVal = 0.0;  ///< 最大值
    double minVal = 0.0;  ///< 最小值
    double avg = 0.0;     ///< 平均值
    double stdDev = 0.0;  ///< 标准差
};

/// 跟踪算法参数
struct TrackingSettings {
    OpticParams opticParams{};                ///< 光学参数
    float thresholdLiving = 0.5f;             ///< 存活判定阈值
    int numFramesLiving = 10;                 ///< 存活判定所需帧数
    float searchRadius = 50.0f;               ///< 搜索半径（像素）
    float ratioFov = 0.25f;                   ///< 视场比例系数
    float thresholdStarMode = 10.0f;          ///< 恒星模式阈值（像素）
    float thresholdGazeMode = 2.0f;           ///< 凝视模式阈值（像素）
    bool autoDecide = true;                   ///< 是否自动选择跟踪模式
    float thresholdMeo = 5.0f;                ///< 中轨目标阈值（像素）
    float spdLowAe = 0.0f;                    ///< 角速度下限（度）
    float spdHighAe = 0.0f;                   ///< 角速度上限（度）
    float thresholdAe = 0.0f;                 ///< 角位置阈值（度）
    bool geoFullLeo = true;                   ///< 地球同步轨道是否覆盖低轨模式
    double geoRaThresholdArcsec = 5.4;        ///< GEO 赤经阈值（角秒）
    double geoDecThresholdArcsec = 3.0;       ///< GEO 赤纬阈值（角秒）
    double geoRaSpeedThresholdArcsec = 10.0;  ///< GEO 赤经速度阈值（角秒）
    double geoDecSpeedThresholdArcsec = 6.0;  ///< GEO 赤纬速度阈值（角秒）
};

/// 曝光与显示同步数据
struct ExposureDisplayData {
    Timestamp timestamp{};        ///< 时间戳
    Vec2f pointingAe{};           ///< 指向方位/俯仰（度）
    float exposureTime = 0.0f;    ///< 曝光时间（秒）
    float frameFrequency = 0.0f;  ///< 帧频（Hz）
    double temperature = 0.0;     ///< 温度
    double atmosPressure = 0.0;   ///< 大气压
    double humidity = 0.0;        ///< 湿度
};

/// 测量结果数据包
struct ResultPacket {
    std::string targetId;            ///< 目标标识
    Timestamp timestamp{};           ///< 时间戳
    uint64_t frameSeq = 0;           ///< 帧序号
    float exposureTime = 0.0f;       ///< 曝光时间（秒）
    float frameFreq = 0.0f;          ///< 帧频（Hz）
    MeasuredBlob blob{};             ///< 测量光斑
    Vec2f fovCenterAe{};             ///< 视场中心方位/俯仰（度）
    Vec2f fovCenterAeModified{};     ///< 修正后的视场中心方位/俯仰（度）
    Vec2f targetPosFrame{};          ///< 目标位置（像素）
    Vec2f targetDistAe{};            ///< 目标角距（度）
    Vec2f targetPosZxdw{};           ///< 目标轴系坐标
    Vec2f targetPosTwdw{};           ///< 目标天文坐标
    float targetMvGdcl = 0.0f;       ///< 目标光度
    float temperature = 10.0f;       ///< 温度
    float humidity = 0.3f;           ///< 湿度
    float atmosPressure = 78900.0f;  ///< 大气压
    bool valid = false;              ///< 测量是否有效
};

/// 指向模型误差解算结果
struct PointingErrorResult {
    double ixA = 0.0;           ///< 方位轴倾斜 X 分量
    double iyA = 0.0;           ///< 方位轴倾斜 Y 分量
    double horizontal = 0.0;    ///< 水平轴误差
    double collimation = 0.0;   ///< 准直误差
    double orientation = 0.0;   ///< 定向误差
    double ixE = 0.0;           ///< 俯仰轴倾斜 X 分量
    double iyE = 0.0;           ///< 俯仰轴倾斜 Y 分量
    double oscillation = 0.0;   ///< 振荡误差
    double zeroOffset = 0.0;    ///< 零点偏移
    double oscillation1 = 0.0;  ///< 振荡误差（第二分量）
};

}  // namespace Dss::Core
