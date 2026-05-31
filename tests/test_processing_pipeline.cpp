#include "dss/processing/processing_pipeline.h"

#include <gtest/gtest.h>

namespace
{

class RecordingStrategy final : public Dss::Processing::IProcessingStrategy
{
public:
    [[nodiscard]] auto process(const Dss::Processing::FramePacket& input)
        -> Dss::Processing::ProcessingResult override
    {
        Dss::Processing::ProcessingResult result;
        result.success = true;
        result.stats.maxVal = static_cast<double>(input.frameSeq);
        return result;
    }

    [[nodiscard]] auto name() const -> std::string_view override
    {
        return "recording";
    }

    [[nodiscard]] auto mode() const -> Dss::Core::ProcessingMode override
    {
        return Dss::Core::ProcessingMode::Direct;
    }
};

} // namespace

TEST(ProcessingPipeline, ReportsEmptyBackendState)
{
    Dss::Processing::ProcessingPipeline pipeline;

    EXPECT_FALSE(pipeline.hasBackend());
    EXPECT_EQ(pipeline.currentMode(), Dss::Core::ProcessingMode::None);
    EXPECT_EQ(pipeline.backendName(), "none");
    EXPECT_FALSE(pipeline.process({}).success);
}

TEST(ProcessingPipeline, DelegatesProcessingToConfiguredBackend)
{
    Dss::Processing::ProcessingPipeline pipeline;
    pipeline.setBackend(std::make_unique<RecordingStrategy>());

    Dss::Processing::FramePacket packet;
    packet.frameSeq = 7;
    const auto result = pipeline.process(packet);

    EXPECT_TRUE(pipeline.hasBackend());
    EXPECT_EQ(pipeline.currentMode(), Dss::Core::ProcessingMode::Direct);
    EXPECT_EQ(pipeline.backendName(), "recording");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.stats.maxVal, 7.0);
}
