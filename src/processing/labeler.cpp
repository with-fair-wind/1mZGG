#include "dss/processing/labeler.h"

#include <algorithm>
#include <numeric>
#include <queue>
#include <vector>

namespace Dss::Processing {

Labeler::Labeler(LabelConfig config) : m_config(config) {}

auto Labeler::labelAndExtract(std::span<const uint8_t> binaryImage, uint32_t width, uint32_t height)
    -> std::vector<Dss::Core::MeasuredBlob> {
    const auto total = static_cast<size_t>(width) * height;
    if (binaryImage.size() != total) {
        return {};
    }

    std::vector<int> labels(total, 0);
    int currentLabel = 0;

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

    std::vector<BlobAccum> accums;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            auto idx = static_cast<size_t>(y) * width + x;
            if (binaryImage[idx] == 0 || labels[idx] != 0) {
                continue;
            }

            ++currentLabel;
            accums.emplace_back();
            auto& acc = accums.back();

            std::queue<std::pair<uint32_t, uint32_t>> queue;
            queue.push({x, y});
            labels[idx] = currentLabel;

            while (!queue.empty()) {
                auto [cx, cy] = queue.front();
                queue.pop();

                auto fx = static_cast<float>(cx);
                auto fy = static_cast<float>(cy);
                float dn = static_cast<float>(binaryImage[static_cast<size_t>(cy) * width + cx]);

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
                        if (nx >= 0 && nx < static_cast<int>(width) && ny >= 0 &&
                            ny < static_cast<int>(height)) {
                            auto nidx = static_cast<size_t>(ny) * width + static_cast<size_t>(nx);
                            if (binaryImage[nidx] > 0 && labels[nidx] == 0) {
                                labels[nidx] = currentLabel;
                                queue.push({static_cast<uint32_t>(nx), static_cast<uint32_t>(ny)});
                            }
                        }
                    }
                }
            }
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
