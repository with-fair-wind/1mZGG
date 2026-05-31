#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "dss/core/constants.h"
#include "dss/core/types.h"

namespace Dss::Core
{

// --- Acquisition events ---
struct FrameAcquiredEvent
{
    uint64_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    ExposureDisplayData metadata{};
};

struct GrabStartedEvent
{
    uint32_t width = 0;
    uint32_t height = 0;
};

struct GrabStoppedEvent
{
};

// --- Processing events ---
struct DisplayRefreshEvent
{
    uint64_t frameSeq = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    std::shared_ptr<const std::vector<uint8_t>> displayImage;
};

struct ProcessingCompleteEvent
{
    uint64_t frameSeq = 0;
    ImageStats stats{};
};

struct RotatedFrameReadyEvent
{
    uint64_t frameSeq = 0;
};

// --- Tracking events ---
struct TrackResultEvent
{
    uint64_t frameSeq = 0;
    std::vector<TargetInfo> targets;
};

// --- Network events ---
struct ImageSendEvent
{
    uint64_t frameSeq = 0;
};

// --- Serial communication events ---
struct MasterControlEvent
{
    float exposure = 0.0f;
    int trackMode = 0;
    bool save = false;
    bool grab = false;
};

struct ExposureSyncEvent
{
    ExposureDisplayData data{};
};

struct Sync25HzEvent
{
};

// --- UI events ---
struct ManualTargetSelectEvent
{
    float x = 0.0f;
    float y = 0.0f;
};

struct ZoomChangeEvent
{
    int level = 0;
};

struct CloseEvent
{
};

// --- System events ---
struct LogMessageEvent
{
    std::string message;
};

struct AtmosphereDataEvent
{
    double temperature = 0.0;
    double pressure = 0.0;
    double humidity = 0.0;
};

} // namespace Dss::Core
