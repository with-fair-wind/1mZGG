#include "dss/processing/opencv_processing_strategy.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace Dss::Processing
{

namespace
{

[[nodiscard]] auto expectedPixelCount(const FramePacket& input) -> std::size_t
{
    return static_cast<std::size_t>(input.width) * static_cast<std::size_t>(input.height);
}

[[nodiscard]] auto makeBlob(const cv::Mat& stats, const cv::Mat& centroids, int label)
    -> Dss::Core::MeasuredBlob
{
    const auto left = stats.at<int>(label, cv::CC_STAT_LEFT);
    const auto top = stats.at<int>(label, cv::CC_STAT_TOP);
    const auto width = stats.at<int>(label, cv::CC_STAT_WIDTH);
    const auto height = stats.at<int>(label, cv::CC_STAT_HEIGHT);
    const auto area = stats.at<int>(label, cv::CC_STAT_AREA);

    Dss::Core::MeasuredBlob blob{};
    blob.centroid.x = static_cast<float>(centroids.at<double>(label, 0));
    blob.centroid.y = static_cast<float>(centroids.at<double>(label, 1));
    blob.minX = static_cast<float>(left);
    blob.minY = static_cast<float>(top);
    blob.maxX = static_cast<float>(left + width - 1);
    blob.maxY = static_cast<float>(top + height - 1);
    blob.area = static_cast<float>(area);
    blob.dn = static_cast<float>(area) * 255.0f;
    return blob;
}

} // namespace

OpenCvProcessingStrategy::OpenCvProcessingStrategy(OpenCvProcessingOptions options)
    : m_options(options)
{
}

auto OpenCvProcessingStrategy::process(const FramePacket& input) -> ProcessingResult
{
    ProcessingResult result{};
    const auto pixelCount = expectedPixelCount(input);
    if (input.width == 0 || input.height == 0 || input.rawImage.size() != pixelCount)
    {
        return result;
    }

    const auto rows = static_cast<int>(input.height);
    const auto cols = static_cast<int>(input.width);
    const cv::Mat raw16(rows, cols, CV_16UC1, const_cast<uint16_t*>(input.rawImage.data()));

    double minValue = 0.0;
    double maxValue = 0.0;
    cv::minMaxLoc(raw16, &minValue, &maxValue);

    cv::Scalar mean{};
    cv::Scalar stddev{};
    cv::meanStdDev(raw16, mean, stddev);

    result.stats.minVal = minValue;
    result.stats.maxVal = maxValue;
    result.stats.avg = mean[0];
    result.stats.stdDev = stddev[0];

    cv::Mat display8;
    if (maxValue > minValue)
    {
        const auto scale = 255.0 / (maxValue - minValue);
        raw16.convertTo(display8, CV_8U, scale, -minValue * scale);
    }
    else
    {
        display8 = cv::Mat::zeros(rows, cols, CV_8UC1);
    }

    result.displayImage.assign(display8.data, display8.data + display8.total());

    const auto threshold = std::clamp(
        mean[0] + (m_options.thresholdSigma * stddev[0]),
        0.0,
        static_cast<double>(std::numeric_limits<uint16_t>::max()));

    cv::Mat binary16;
    cv::threshold(raw16, binary16, threshold, 255.0, cv::THRESH_BINARY);

    cv::Mat binary8;
    binary16.convertTo(binary8, CV_8U);

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const auto labelCount = cv::connectedComponentsWithStats(binary8, labels, stats, centroids, 8, CV_32S);

    for (int label = 1; label < labelCount; ++label)
    {
        const auto area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area < m_options.minArea || area > m_options.maxArea)
        {
            continue;
        }
        result.targetBlobs.push_back(makeBlob(stats, centroids, label));
    }

    result.success = true;
    return result;
}

auto OpenCvProcessingStrategy::name() const -> std::string_view
{
    return "opencv";
}

auto OpenCvProcessingStrategy::mode() const -> Dss::Core::ProcessingMode
{
    return Dss::Core::ProcessingMode::Direct;
}

} // namespace Dss::Processing
