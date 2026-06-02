#pragma once

#include <cstdint>
#include <numbers>

namespace Dss::Core {

// --- Init file read status ---
enum class InitStatus : int {
    Ok = 1,
    ErrorCommNetSettings = 0,
    ErrorPath = -1,
    ErrorCameraSettings = -2,
    ErrorDisplaySettings = -3,
    ErrorTrackSettings = -4,
    ErrorOpticSettings = -5,
    ErrorFileInvalid = -6,
};

// --- Tri-state operational status ---
enum class Status : int {
    Init = -1,
    Error = 0,
    Ok = 1,
};

// --- Camera trigger mode ---
enum class TriggerMode : int {
    External = 0,
    Free = 1,
};

// --- Serial display mode ---
enum class CommDisplayMode : int {
    PortInit = 0,
    Recv = 1,
    Send = 2,
    RecvCheck = 3,
};

// --- Image processing mode ---
enum class ProcessingMode : int {
    None = 0,
    Diff = 1,
    Direct = 3,
};

// --- Tracking mode ---
enum class TrackMode : int {
    Init = -1,
    Geo = 0,
    SpaceCatalog = 3,
    Leo = 4,
    Manual = 5,
};

// --- Display zoom ratios (percentage of original * 100) ---
inline constexpr int BiggerFirst = 36;
inline constexpr int BiggerSecond = 21;

// --- Astronomical constants ---
inline constexpr double Pi = std::numbers::pi;
inline constexpr double DegToRad = Pi / 180.0;
inline constexpr double RadToDeg = 180.0 / Pi;
inline constexpr double ArcSecToRad = Pi / (180.0 * 3600.0);
inline constexpr double RadToArcSec = (180.0 * 3600.0) / Pi;
inline constexpr double SecToRad = Pi / (12.0 * 3600.0);
inline constexpr double SolarSiderealRatio = 1.00273790935;

// --- Serial protocol constants ---
inline constexpr uint8_t FrameHeader = 0x7E;
inline constexpr uint8_t FrameTail = 0xE7;

}  // namespace Dss::Core
