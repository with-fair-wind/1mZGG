#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Dss::Core {

// --- Timestamp ---
struct Timestamp {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
    int microsecond = 0;
};

struct TimeOfDay {
    int hour = 0;
    int minute = 0;
    int second = 0;
};

// --- Optics parameters ---
struct OpticParams {
    int imageWidth = 6144;
    int imageHeight = 6144;
    float fovCenterX = 6144 / 2.0f;
    float fovCenterY = 6144 / 2.0f;
    float pixelScale = 0.0003453f;  // deg/pixel
};

// --- 2D position pair ---
struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec2d {
    double x = 0.0;
    double y = 0.0;
};

// --- Single blob detection result ---
struct MeasuredBlob {
    std::string id;
    Vec2f centroid{};
    float maxX = 0.0f;
    float minX = 0.0f;
    float maxY = 0.0f;
    float minY = 0.0f;
    float dn = 0.0f;    // grayscale sum
    float area = 0.0f;  // pixel^2
    Vec2f posAe{};      // azimuth/elevation (deg)

    float fovCenterAziModify = 0.0f;
    float fovCenterEleModify = 0.0f;
    float distAzi = 0.0f;
    float distEle = 0.0f;
    float targetAzi = 0.0f;
    float targetEle = 0.0f;
    double pointErrEle = 0.0;

    double alpha = 0.0;
    double sigma = 0.0;
    double ra = 0.0;
    double dec = 0.0;
};

// --- All detections in a single frame ---
struct FrameMeasurements {
    Timestamp timestamp{};
    uint64_t frameSeq = 0;
    Vec2f fovCenterAe{};
    float exposureTime = 0.0f;  // seconds
    float frameFreq = 0.0f;     // Hz
    std::vector<MeasuredBlob> targetBlobs;
    std::vector<MeasuredBlob> starBlobs;
    double temperature = 0.0;
    double atmosPressure = 0.0;
};

// --- Per-frame target state ---
struct TargetFrameInfo {
    Timestamp timestamp{};
    uint64_t frameSeq = 0;
    MeasuredBlob measuredBlob{};
    Vec2f posZxdw{};  // axis positioning
    Vec2f posTwdw{};  // astronomical positioning
    float magnitude = 0.0f;
    bool valid = false;
};

// --- Single target tracking state across frames ---
struct TargetInfo {
    std::string targetId;
    std::string saveStartTime;
    std::string filenameGae;
    std::vector<TargetFrameInfo> frameInfos;
    Vec2f predictedPosFrame{};  // pixel
    Vec2f predictedPosAe{};     // degrees
    Vec2f predictedSpdFrame{};  // pixel/frame
    Vec2f predictedSpdAe{};     // deg/s
    float validity = 1.0f;      // 0~1
    bool living = false;
    Vec2f lastRmDm{};
};

// --- Image statistics ---
struct ImageStats {
    double maxVal = 0.0;
    double minVal = 0.0;
    double avg = 0.0;
    double stdDev = 0.0;
};

// --- Tracking settings ---
struct TrackingSettings {
    OpticParams opticParams{};
    float thresholdLiving = 0.5f;
    int numFramesLiving = 10;
    float searchRadius = 50.0f;  // pixels
    float ratioFov = 0.25f;
    float thresholdStarMode = 10.0f;  // pixels
    float thresholdGazeMode = 2.0f;   // pixels
    bool autoDecide = true;
    float thresholdMeo = 5.0f;  // pixels
    float spdLowAe = 0.0f;      // deg
    float spdHighAe = 0.0f;     // deg
    float thresholdAe = 0.0f;   // deg
};

// --- Exposure/display synchronized data ---
struct ExposureDisplayData {
    Timestamp timestamp{};
    Vec2f pointingAe{};  // azimuth/elevation
    float exposureTime = 0.0f;
    float frameFrequency = 0.0f;
    double temperature = 0.0;
    double atmosPressure = 0.0;
    double humidity = 0.0;
};

// --- Measurement result packet ---
struct ResultPacket {
    std::string targetId;
    Timestamp timestamp{};
    uint64_t frameSeq = 0;
    float exposureTime = 0.0f;
    float frameFreq = 0.0f;
    MeasuredBlob blob{};
    Vec2f fovCenterAe{};
    Vec2f fovCenterAeModified{};
    Vec2f targetPosFrame{};
    Vec2f targetDistAe{};
    Vec2f targetPosZxdw{};
    Vec2f targetPosTwdw{};
    float targetMvGdcl = 0.0f;
    float temperature = 10.0f;
    float humidity = 0.3f;
    float atmosPressure = 78900.0f;
    bool valid = false;
};

// --- Error result (pointing model) ---
struct PointingErrorResult {
    double ixA = 0.0;
    double iyA = 0.0;
    double horizontal = 0.0;
    double collimation = 0.0;
    double orientation = 0.0;
    double ixE = 0.0;
    double iyE = 0.0;
    double oscillation = 0.0;
    double zeroOffset = 0.0;
    double oscillation1 = 0.0;
};

}  // namespace Dss::Core
