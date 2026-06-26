#include "dss/core/result_packet_utils.h"

#include <memory>
#include <utility>

namespace Dss::Core {

auto latestTargetFrameInfo(const TargetInfo& target) -> const TargetFrameInfo* {
    if (target.frameInfos.empty()) {
        return nullptr;
    }
    return std::addressof(target.frameInfos.back());
}

auto makeResultPacket(const TargetInfo& target) -> std::optional<ResultPacket> {
    const auto* frame = latestTargetFrameInfo(target);
    if (frame == nullptr) {
        return std::nullopt;
    }

    const auto& blob = frame->measuredBlob;
    ResultPacket packet{};
    packet.targetId = target.targetId.empty() ? blob.id : target.targetId;
    packet.timestamp = frame->timestamp;
    packet.frameSeq = frame->frameSeq;
    packet.exposureTime = frame->exposureTime;
    packet.frameFreq = frame->frameFreq;
    packet.blob = blob;
    packet.fovCenterAe = frame->fovCenterAe;
    packet.fovCenterAeModified = Vec2f{blob.fovCenterAziModify, blob.fovCenterEleModify};
    packet.opticCenter = frame->opticCenter;
    packet.targetPosFrame = blob.centroid;
    packet.targetDistAe = Vec2f{blob.distAzi, blob.distEle};
    packet.targetSpdAe = target.predictedSpdAe;
    packet.targetPosZxdw = frame->posZxdw;
    packet.targetPosTwdw = frame->posTwdw;
    packet.targetMvGdcl = frame->magnitude;
    packet.valid = frame->valid;
    return packet;
}

auto makeResultPackets(std::span<const TargetInfo> targets) -> std::vector<ResultPacket> {
    std::vector<ResultPacket> packets;
    packets.reserve(targets.size());
    for (const auto& target : targets) {
        if (auto packet = makeResultPacket(target); packet.has_value()) {
            packets.push_back(std::move(*packet));
        }
    }
    return packets;
}

}  // namespace Dss::Core
