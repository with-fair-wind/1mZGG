#include "dss/processing/detail/labeler.h"

#include <algorithm>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

namespace Dss::Processing {

namespace {
struct BlobAccum {
    float sumX = 0;
    float sumY = 0;
    float sumDn = 0;
    float area = 0;
    float maxX = -1e9f;
    float minX = 1e9f;
    float maxY = -1e9f;
    float minY = 1e9f;
};

/// @brief 从已发现的前景种子累积一个八连通光斑；labels 与输入尺寸保持一致。
/// @param binaryImage 二值输入。
/// @param labels 当前标签缓冲。
/// @param width 图像宽度。
/// @param height 图像高度。
/// @param x 种子横坐标。
/// @param y 种子纵坐标。
/// @param currentLabel 当前连通域标签。
/// @return 当前连通域的统计量。
auto accumulateBlob(std::span<const uint8_t> binaryImage, std::span<int> labels, uint32_t width,
                    uint32_t height, uint32_t x, uint32_t y, int currentLabel) -> BlobAccum {
    BlobAccum acc;
    std::queue<std::pair<uint32_t, uint32_t>> queue;
    queue.emplace(x, y);
    labels[(static_cast<std::size_t>(y) * width) + x] = currentLabel;

    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop();

        auto fx = static_cast<float>(cx);
        auto fy = static_cast<float>(cy);
        auto dn = static_cast<float>(binaryImage[(static_cast<size_t>(cy) * width) + cx]);

        acc.sumX += fx * dn;
        acc.sumY += fy * dn;
        acc.sumDn += dn;
        acc.area += 1.0f;
        acc.maxX = std::max(acc.maxX, fx);
        acc.minX = std::min(acc.minX, fx);
        acc.maxY = std::max(acc.maxY, fy);
        acc.minY = std::min(acc.minY, fy);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                auto nx = static_cast<int>(cx) + dx;
                auto ny = static_cast<int>(cy) + dy;
                if (nx >= 0 && std::cmp_less(nx, width) && ny >= 0 && std::cmp_less(ny, height)) {
                    auto nidx = (static_cast<size_t>(ny) * width) + static_cast<size_t>(nx);
                    if (binaryImage[nidx] > 0 && labels[nidx] == 0) {
                        labels[nidx] = currentLabel;
                        queue.emplace(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
                    }
                }
            }
        }
    }
    return acc;
}
}  // namespace

Labeler::Labeler(LabelConfig config) : m_config(config) {}

auto Labeler::labelAndExtract(std::span<const uint8_t> binaryImage, uint32_t width,
                              uint32_t height) const -> std::vector<Dss::Core::MeasuredBlob> {
    const auto total = static_cast<size_t>(width) * height;
    if (binaryImage.size() != total) {
        return {};
    }

    std::vector<int> labels(total, 0);
    int currentLabel = 0;

    std::vector<BlobAccum> accums;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            auto idx = (static_cast<size_t>(y) * width) + x;
            if (binaryImage[idx] == 0 || labels[idx] != 0) {
                continue;
            }

            ++currentLabel;
            accums.push_back(
                accumulateBlob(binaryImage, labels, width, height, x, y, currentLabel));
        }
    }

    std::vector<Dss::Core::MeasuredBlob> blobs;
    for (const auto& acc : accums) {
        auto area = static_cast<int>(acc.area);
        if (area < m_config.minArea || area > m_config.maxArea) {
            continue;
        }

        Dss::Core::MeasuredBlob blob{};
        blob.centroid.x = (acc.sumDn > 0) ? acc.sumX / acc.sumDn : 0.0f;
        blob.centroid.y = (acc.sumDn > 0) ? acc.sumY / acc.sumDn : 0.0f;
        blob.dn = acc.sumDn;
        blob.area = acc.area;
        blob.maxX = acc.maxX;
        blob.minX = acc.minX;
        blob.maxY = acc.maxY;
        blob.minY = acc.minY;
        blobs.push_back(blob);
    }

    return blobs;
}

}  // namespace Dss::Processing
