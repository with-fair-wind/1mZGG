#include "tracking_fixture_loader.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include <nlohmann/json.hpp>

namespace Dss::Tests {
namespace {

using Json = nlohmann::json;

[[nodiscard]] auto fail(std::string message) -> std::expected<TrackingFixture, std::string> {
    return std::unexpected(std::move(message));
}

[[nodiscard]] auto finiteNumber(const Json& value, std::string_view path) -> float {
    if (!value.is_number()) {
        throw std::runtime_error(std::string(path) + " must be a number");
    }
    const auto number = value.get<double>();
    if (!std::isfinite(number) || number < -std::numeric_limits<float>::max() ||
        number > std::numeric_limits<float>::max()) {
        throw std::runtime_error(std::string(path) + " must be a finite float");
    }
    return static_cast<float>(number);
}

[[nodiscard]] auto vec2(const Json& value, std::string_view path) -> Dss::Core::Vec2f {
    if (!value.is_array() || value.size() != 2U) {
        throw std::runtime_error(std::string(path) + " must contain two numbers");
    }
    return {finiteNumber(value[0], std::string(path) + "[0]"),
            finiteNumber(value[1], std::string(path) + "[1]")};
}

[[nodiscard]] auto blob(const Json& value, std::string_view path) -> Dss::Core::MeasuredBlob {
    if (!value.is_object() || !value.contains("centroid")) {
        throw std::runtime_error(std::string(path) + ".centroid is required");
    }

    Dss::Core::MeasuredBlob result{};
    result.centroid = vec2(value.at("centroid"), std::string(path) + ".centroid");
    result.id = value.value("id", std::string{});
    result.minX = value.contains("min_x") ? finiteNumber(value.at("min_x"), "blob.min_x")
                                          : result.centroid.x - 1.0F;
    result.maxX = value.contains("max_x") ? finiteNumber(value.at("max_x"), "blob.max_x")
                                          : result.centroid.x + 1.0F;
    result.minY = value.contains("min_y") ? finiteNumber(value.at("min_y"), "blob.min_y")
                                          : result.centroid.y - 1.0F;
    result.maxY = value.contains("max_y") ? finiteNumber(value.at("max_y"), "blob.max_y")
                                          : result.centroid.y + 1.0F;
    result.area = value.contains("area") ? finiteNumber(value.at("area"), "blob.area") : 4.0F;
    result.dn = value.contains("dn") ? finiteNumber(value.at("dn"), "blob.dn") : 100.0F;
    result.magnitude =
        value.contains("magnitude") ? finiteNumber(value.at("magnitude"), "blob.magnitude") : 0.0F;
    result.posAe = value.contains("pos_ae")
                       ? vec2(value.at("pos_ae"), std::string(path) + ".pos_ae")
                       : Dss::Core::Vec2f{};
    result.alpha = value.value("alpha", 0.0);
    result.sigma = value.value("sigma", 0.0);
    result.ra = value.value("ra", 0.0);
    result.dec = value.value("dec", 0.0);
    if (result.area < 0.0F || result.dn < 0.0F || result.maxX < result.minX ||
        result.maxY < result.minY || !std::isfinite(result.alpha) || !std::isfinite(result.sigma) ||
        !std::isfinite(result.ra) || !std::isfinite(result.dec)) {
        throw std::runtime_error(std::string(path) + " contains invalid numeric bounds");
    }
    return result;
}

void readSettings(const Json& value, Dss::Core::TrackingSettings& settings) {
    if (!value.is_object()) {
        throw std::runtime_error("settings must be an object");
    }
    const auto setFloat = [&](std::string_view name, float& destination) {
        const auto key = std::string(name);
        if (value.contains(key)) {
            destination = finiteNumber(value.at(key), "settings." + key);
        }
    };
    setFloat("search_radius", settings.searchRadius);
    setFloat("threshold_meo", settings.thresholdMeo);
    setFloat("threshold_ae", settings.thresholdAe);
    setFloat("spd_low_ae", settings.spdLowAe);
    setFloat("spd_high_ae", settings.spdHighAe);
    setFloat("threshold_living", settings.thresholdLiving);
    setFloat("threshold_gaze_mode", settings.thresholdGazeMode);
    setFloat("threshold_star_mode", settings.thresholdStarMode);
    setFloat("ratio_fov", settings.ratioFov);
    if (value.contains("num_frames_living")) {
        settings.numFramesLiving = value.at("num_frames_living").get<int>();
        if (settings.numFramesLiving <= 0) {
            throw std::runtime_error("settings.num_frames_living must be positive");
        }
    }
    if (value.contains("optic")) {
        const auto& optic = value.at("optic");
        if (!optic.is_object()) {
            throw std::runtime_error("settings.optic must be an object");
        }
        settings.opticParams.imageWidth =
            optic.value("image_width", settings.opticParams.imageWidth);
        settings.opticParams.imageHeight =
            optic.value("image_height", settings.opticParams.imageHeight);
        if (optic.contains("center")) {
            const auto center = vec2(optic.at("center"), "settings.optic.center");
            settings.opticParams.fovCenterX = center.x;
            settings.opticParams.fovCenterY = center.y;
        }
        if (optic.contains("pixel_scale")) {
            settings.opticParams.pixelScale =
                finiteNumber(optic.at("pixel_scale"), "settings.optic.pixel_scale");
        }
        if (settings.opticParams.imageWidth <= 0 || settings.opticParams.imageHeight <= 0 ||
            settings.opticParams.pixelScale <= 0.0F) {
            throw std::runtime_error("settings.optic dimensions and pixel_scale must be positive");
        }
    }
}

[[nodiscard]] auto modeFrom(std::string_view value) -> TrackingFixtureMode {
    if (value == "geo")
        return TrackingFixtureMode::Geo;
    if (value == "leo")
        return TrackingFixtureMode::Leo;
    if (value == "sc")
        return TrackingFixtureMode::Sc;
    if (value == "manual")
        return TrackingFixtureMode::Manual;
    throw std::runtime_error("mode must be geo, leo, sc, or manual");
}

[[nodiscard]] auto expectedFrame(const Json& value, std::string_view path)
    -> ExpectedTrackingFrame {
    if (!value.is_object() || !value.contains("target_count")) {
        throw std::runtime_error(std::string(path) + ".target_count is required");
    }
    ExpectedTrackingFrame expected{};
    expected.targetCount = value.at("target_count").get<std::size_t>();
    if (!value.contains("targets")) {
        return expected;
    }
    for (const auto& item : value.at("targets")) {
        ExpectedTrackingTarget target{};
        target.targetId = item.at("id").get<std::string>();
        target.living = item.at("living").get<bool>();
        target.historySize = item.at("history_size").get<std::size_t>();
        target.latestValid = item.at("latest_valid").get<bool>();
        target.posTwdw = vec2(item.at("twdw"), std::string(path) + ".targets.twdw");
        const auto& gdcl = item.at("gdcl");
        target.dn = finiteNumber(gdcl.at("dn"), std::string(path) + ".targets.gdcl.dn");
        target.magnitude =
            finiteNumber(gdcl.at("magnitude"), std::string(path) + ".targets.gdcl.magnitude");
        if (target.targetId.empty() || target.historySize == 0U) {
            throw std::runtime_error(std::string(path) + ".targets has an empty id or history");
        }
        expected.targets.push_back(std::move(target));
    }
    if (expected.targets.size() > expected.targetCount) {
        throw std::runtime_error(std::string(path) + ".targets exceeds target_count");
    }
    return expected;
}

}  // namespace

auto parseTrackingFixture(std::string_view jsonText)
    -> std::expected<TrackingFixture, std::string> {
    try {
        const auto root = Json::parse(jsonText);
        if (!root.is_object()) {
            return fail("fixture root must be a JSON object");
        }
        if (root.value("schema_version", 0) != 1) {
            return fail("schema_version must be 1");
        }
        if (!root.contains("name") || !root.at("name").is_string() ||
            root.at("name").get_ref<const std::string&>().empty()) {
            return fail("name is required");
        }
        if (!root.contains("mode") || !root.at("mode").is_string()) {
            return fail("mode is required");
        }
        if (!root.contains("frames") || !root.at("frames").is_array() ||
            root.at("frames").empty()) {
            return fail("frames must be a non-empty array");
        }

        TrackingFixture fixture{};
        fixture.name = root.at("name").get<std::string>();
        fixture.mode = modeFrom(root.at("mode").get<std::string>());
        if (root.contains("settings")) {
            readSettings(root.at("settings"), fixture.settings);
        }

        std::uint64_t previousSequence = 0;
        for (std::size_t index = 0; index < root.at("frames").size(); ++index) {
            const auto& item = root.at("frames")[index];
            const auto path = "frames[" + std::to_string(index) + "]";
            if (!item.is_object() || !item.contains("frame_seq") ||
                !item.at("frame_seq").is_number_unsigned()) {
                return fail(path + ".frame_seq must be an unsigned integer");
            }
            const auto sequence = item.at("frame_seq").get<std::uint64_t>();
            if (sequence == 0 || sequence <= previousSequence) {
                return fail(path + ".frame_seq must be positive and strictly increasing");
            }
            previousSequence = sequence;

            TrackingFixtureFrame frame{};
            frame.measurements.frameSeq = sequence;
            frame.measurements.frameFreq =
                item.contains("frame_freq")
                    ? finiteNumber(item.at("frame_freq"), path + ".frame_freq")
                    : 1.0F;
            frame.measurements.fovCenterAe =
                item.contains("fov_center_ae")
                    ? vec2(item.at("fov_center_ae"), path + ".fov_center_ae")
                    : Dss::Core::Vec2f{};
            frame.measurements.timestamp.second = static_cast<int>((sequence - 1U) % 60U);
            for (const auto& value : item.value("target_blobs", Json::array())) {
                frame.measurements.targetBlobs.push_back(blob(value, path + ".target_blobs"));
            }
            for (const auto& value : item.value("star_blobs", Json::array())) {
                frame.measurements.starBlobs.push_back(blob(value, path + ".star_blobs"));
            }
            if (item.contains("manual_target")) {
                frame.manualTarget = blob(item.at("manual_target"), path + ".manual_target");
            }
            frame.reset = item.value("reset", false);
            if (!item.contains("expected")) {
                return fail(path + ".expected is required");
            }
            frame.expected = expectedFrame(item.at("expected"), path + ".expected");
            fixture.frames.push_back(std::move(frame));
        }
        return fixture;
    } catch (const nlohmann::json::exception& error) {
        return fail(std::string("JSON parse/field error: ") + error.what());
    } catch (const std::exception& error) {
        return fail(error.what());
    }
}

auto loadTrackingFixture(const std::filesystem::path& path)
    -> std::expected<TrackingFixture, std::string> {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fail("cannot open fixture: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseTrackingFixture(buffer.str());
}

}  // namespace Dss::Tests
