#include "dss/ui/processing_view_model.h"

#include <memory>

#include "dss/core/config.h"
#include "dss/processing/diff_processing_strategy.h"
#include "dss/processing/image_processor.h"
#ifdef DSS_HAS_OPENCV
#include "dss/processing/opencv_processing_strategy.h"
#endif

namespace Dss::Ui {

ProcessingViewModel::ProcessingViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_registry(context.registry) {}

ProcessingViewModel::~ProcessingViewModel() = default;

int ProcessingViewModel::processingMode() const {
    return m_processingMode;
}

void ProcessingViewModel::setProcessingMode(int mode) {
    if (m_processingMode != mode) {
        m_processingMode = mode;
        Q_EMIT processingModeChanged(mode);
    }
    configureProcessingStrategy();
}

void ProcessingViewModel::configureProcessingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        Q_EMIT statusTextChanged("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::ProcessingMode>(m_processingMode);
    const auto options = Dss::Core::Config::instance().processing();
    switch (mode) {
        case Dss::Core::ProcessingMode::None:
            processor->setProcessingStrategy(nullptr);
            Q_EMIT statusTextChanged("Processing disabled");
            break;
        case Dss::Core::ProcessingMode::Direct:
#ifdef DSS_HAS_OPENCV
            processor->setProcessingStrategy(
                std::make_unique<Dss::Processing::OpenCvProcessingStrategy>(
                    Dss::Processing::OpenCvProcessingOptions{
                        .thresholdSigma = options.thresholdSigma,
                        .minArea = options.minArea,
                        .maxArea = options.maxArea,
                        .displayLow = options.displayLow,
                        .displayHigh = options.displayHigh,
                    }));
            Q_EMIT statusTextChanged("OpenCV processing enabled");
#else
            processor->setProcessingStrategy(nullptr);
            Q_EMIT statusTextChanged("OpenCV processing is not available");
#endif
            break;
        case Dss::Core::ProcessingMode::Diff:
            processor->setProcessingStrategy(
                std::make_unique<Dss::Processing::DiffProcessingStrategy>(
                    Dss::Processing::DiffProcessingOptions{
                        .threshold = options.diffThreshold,
                        .minArea = options.minArea,
                        .maxArea = options.maxArea,
                    }));
            Q_EMIT statusTextChanged("Diff processing enabled");
            break;
    }
}

}  // namespace Dss::Ui
