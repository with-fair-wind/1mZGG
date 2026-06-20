#include <QCoreApplication>
#include <QMetaObject>
#include <QVariant>
#include <memory>

#include <gtest/gtest.h>

#include "dss/processing/image_processor.h"
#include "dss/ui/processing_view_model.h"
#include "dss/ui/view_model_context.h"

namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_processing_view_model";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

}  // namespace

TEST(ProcessingViewModel, SetProcessingModeConfiguresImageProcessorBackend) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::ProcessingViewModel processing(
        Dss::Ui::UiServiceContext{.bus = bus, .registry = registry});

    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::None);

    const auto directInvoked =
        QMetaObject::invokeMethod(&processing, "setProcessingMode",
                                  Q_ARG(int, static_cast<int>(Dss::Core::ProcessingMode::Direct)));
    EXPECT_TRUE(directInvoked);
    EXPECT_EQ(processing.property("processingMode").toInt(),
              static_cast<int>(Dss::Core::ProcessingMode::Direct));
    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::Direct);

    const auto diffInvoked =
        QMetaObject::invokeMethod(&processing, "setProcessingMode",
                                  Q_ARG(int, static_cast<int>(Dss::Core::ProcessingMode::Diff)));
    EXPECT_TRUE(diffInvoked);
    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::Diff);

    const auto noneInvoked =
        QMetaObject::invokeMethod(&processing, "setProcessingMode",
                                  Q_ARG(int, static_cast<int>(Dss::Core::ProcessingMode::None)));
    EXPECT_TRUE(noneInvoked);
    EXPECT_EQ(processing.property("processingMode").toInt(),
              static_cast<int>(Dss::Core::ProcessingMode::None));
    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::None);
}
