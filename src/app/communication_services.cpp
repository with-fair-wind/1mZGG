#include "dss/app/application_context.h"

#ifdef DSS_BUILD_APP
#include <filesystem>
#include <format>
#include <memory>

#include "dss/acquisition/i_camera_controller.h"
#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/comm/display_channel.h"
#include "dss/comm/exposure_channel.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/comm/master_control_channel.h"
#include "dss/comm/servo_channel.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/image_sender.h"
#include "dss/processing/image_processor.h"
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"
#endif

namespace Dss::App {

void ApplicationContext::registerCommunicationServices() {
#ifdef DSS_BUILD_APP
    auto display = std::make_shared<Dss::Comm::DisplayChannel>(m_bus);
    auto exposure = std::make_shared<Dss::Comm::ExposureChannel>(m_bus);
    auto masterControl = std::make_shared<Dss::Comm::MasterControlChannel>(m_bus);
    auto servo = std::make_shared<Dss::Comm::ServoChannel>(m_bus);

    m_registry.registerService<Dss::Comm::ISerialChannel>("display", std::move(display));
    m_registry.registerService<Dss::Comm::ISerialChannel>("exposure", std::move(exposure));
    m_registry.registerService<Dss::Comm::ISerialChannel>("master_control",
                                                          std::move(masterControl));
    m_registry.registerService<Dss::Comm::ISerialChannel>("servo", std::move(servo));

    m_registry.registerService<Dss::Network::ImageSender>(
        "image_sender", std::make_shared<Dss::Network::ImageSender>(m_bus));
    m_registry.registerService<Dss::Network::Heartbeat>(
        "heartbeat", std::make_shared<Dss::Network::Heartbeat>(m_bus));
    m_registry.registerService<Dss::Network::ErrorDiagnostics>(
        "error_diagnostics", std::make_shared<Dss::Network::ErrorDiagnostics>(m_bus));
    m_registry.registerService<Dss::Network::DataExchange>(
        "data_exchange", std::make_shared<Dss::Network::DataExchange>(m_bus));
    m_registry.registerService<Dss::Network::AtmosReceiver>(
        "atmos_receiver", std::make_shared<Dss::Network::AtmosReceiver>(m_bus));
    m_registry.registerService<Dss::Acquisition::ICameraController>(
        "camera", std::make_shared<Dss::Acquisition::CommandOnlyCameraController>(
                      Dss::Core::Config::instance().commNet().cameraPort));
    auto localImageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(
        Dss::Core::Config::instance().paths().dataRoot);
    auto imageProcessor = std::make_shared<Dss::Processing::ImageProcessor>(m_bus);
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    replaySource->setFrameCallback([imageProcessor,
                                    localImageStorage](Dss::Processing::FramePacket packet) {
        if (localImageStorage->isRunning() && !packet.rawImage.empty()) {
            Dss::Storage::RawImageMetadata metadata{};
            metadata.width = packet.width;
            metadata.height = packet.height;
            metadata.exposure = packet.metadata;
            metadata.exposureTimeMilliseconds =
                static_cast<double>(packet.metadata.exposureTime) * 1000.0;
            metadata.frameFrequency = packet.metadata.frameFrequency;
            (void)localImageStorage->enqueueRawFrame(
                std::filesystem::path("replay") / std::format("frame_{:06}.raw", packet.frameSeq),
                metadata, packet.rawImage);
        }
        (void)imageProcessor->submitFrame(std::move(packet));
    });

    m_registry.registerService<Dss::Processing::ImageProcessor>("image_processor", imageProcessor);
    m_registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                           replaySource);
    m_registry.registerService<Dss::Acquisition::IFrameSource>("replay_source",
                                                               std::move(replaySource));
    m_registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage",
                                                                       localImageStorage);
    m_registry.registerService<Dss::Storage::IStorageBackend>("image_storage",
                                                              std::move(localImageStorage));
    m_registry.registerService<Dss::Storage::IStorageBackend>(
        "track_data_storage", std::make_shared<Dss::Storage::TrackDataStorageBackend>(
                                  Dss::Core::Config::instance().paths().dataRoot));
#endif
}

}  // namespace Dss::App
