#include <QCoreApplication>
#include <QMetaObject>
#include <QVariant>
#include <memory>

#include <gtest/gtest.h>

#include "dss/processing/image_processor.h"
#include "dss/ui/view_model.h"

namespace {

auto ensureApplication() -> QCoreApplication& {
    static int argc = 1;
    static char appName[] = "test_view_model_processing";
    static char* argv[] = {appName, nullptr};
    static std::unique_ptr<QCoreApplication> app;

    if (QCoreApplication::instance() == nullptr) {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    return *QCoreApplication::instance();
}

}  // namespace

TEST(ViewModelProcessing, SetProcessingModeConfiguresImageProcessorBackend) {
    auto& app = ensureApplication();
    (void)app;

    Dss::Processing::ImageProcessor::MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    auto processor = std::make_shared<Dss::Processing::ImageProcessor>(bus);
    registry.registerService<Dss::Processing::ImageProcessor>("image_processor", processor);
    Dss::Ui::ViewModel viewModel(bus, registry);

    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::None);

    const auto directInvoked =
        QMetaObject::invokeMethod(&viewModel, "setProcessingMode",
                                  Q_ARG(int, static_cast<int>(Dss::Core::ProcessingMode::Direct)));
    EXPECT_TRUE(directInvoked);
    EXPECT_EQ(viewModel.property("processingMode").toInt(),
              static_cast<int>(Dss::Core::ProcessingMode::Direct));
    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::Direct);

    const auto noneInvoked =
        QMetaObject::invokeMethod(&viewModel, "setProcessingMode",
                                  Q_ARG(int, static_cast<int>(Dss::Core::ProcessingMode::None)));
    EXPECT_TRUE(noneInvoked);
    EXPECT_EQ(viewModel.property("processingMode").toInt(),
              static_cast<int>(Dss::Core::ProcessingMode::None));
    EXPECT_EQ(processor->currentProcessingMode(), Dss::Core::ProcessingMode::None);
}
