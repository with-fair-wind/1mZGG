#include "dss/app/application_context.h"

#ifdef DSS_BUILD_APP
#include <memory>

#include "dss/acquisition/i_camera_controller.h"
#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/app/track_result_data_exchange_bridge.h"
#include "dss/comm/display_channel.h"
#include "dss/comm/exposure_channel.h"
#include "dss/comm/i_serial_channel.h"
#include "dss/comm/master_control_channel.h"
#include "dss/comm/serial_command_interfaces.h"
#include "dss/comm/servo_channel.h"
#include "dss/network/atmos_receiver.h"
#include "dss/network/data_exchange.h"
#include "dss/network/error_diagnostics.h"
#include "dss/network/heartbeat.h"
#include "dss/network/i_network_channel.h"
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

    m_registry.registerService<Dss::Comm::ISerialChannel>("display", display);
    m_registry.registerService<Dss::Comm::ISerialChannel>("exposure", exposure);
    m_registry.registerService<Dss::Comm::IExposureCommandPort>("exposure", exposure);
    m_registry.registerService<Dss::Comm::ISerialChannel>("master_control", masterControl);
    m_registry.registerService<Dss::Comm::IMasterControlStatusPort>("master_control",
                                                                    masterControl);
    m_registry.registerService<Dss::Comm::ISerialChannel>("servo", servo);
    m_registry.registerService<Dss::Comm::IServoCorrectionPort>("servo", servo);

    auto imageSender = std::make_shared<Dss::Network::ImageSender>(m_bus);
    auto heartbeat = std::make_shared<Dss::Network::Heartbeat>(m_bus);
    auto errorDiagnostics = std::make_shared<Dss::Network::ErrorDiagnostics>(m_bus);
    auto atmosReceiver = std::make_shared<Dss::Network::AtmosReceiver>(m_bus);

    m_registry.registerService<Dss::Network::ImageSender>("image_sender", imageSender);
    m_registry.registerService<Dss::Network::INetworkChannel>("image_sender", imageSender);
    m_registry.registerService<Dss::Network::Heartbeat>("heartbeat", heartbeat);
    m_registry.registerService<Dss::Network::INetworkChannel>("heartbeat", heartbeat);
    m_registry.registerService<Dss::Network::ErrorDiagnostics>("error_diagnostics",
                                                               errorDiagnostics);
    m_registry.registerService<Dss::Network::INetworkChannel>("error_diagnostics",
                                                              errorDiagnostics);
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(m_bus);
    m_registry.registerService<Dss::Network::DataExchange>("data_exchange", dataExchange);
    auto trackResultDataExchangeBridge = std::make_shared<Dss::App::TrackResultDataExchangeBridge>(
        m_bus,
        [dataExchange](const Dss::Network::GxtcMetadata& metadata,
                       std::span<const Dss::Network::GxtcTarget> targets) {
            if (dataExchange->isOpen()) {
                (void)dataExchange->sendGxtc(metadata, targets);
            }
        },
        [dataExchange](const Dss::Network::GdclMeasurement& measurement) {
            if (dataExchange->isOpen()) {
                (void)dataExchange->sendGdcl(measurement);
            }
        });
    m_registry.registerService<Dss::App::TrackResultDataExchangeBridge>(
        "track_result_data_exchange_bridge", std::move(trackResultDataExchangeBridge));
    m_registry.registerService<Dss::Network::AtmosReceiver>("atmos_receiver", atmosReceiver);
    m_registry.registerService<Dss::Network::INetworkChannel>("atmos_receiver", atmosReceiver);
    m_registry.registerService<Dss::Acquisition::ICameraController>(
        "camera", std::make_shared<Dss::Acquisition::CommandOnlyCameraController>(
                      Dss::Core::Config::instance().commNet().cameraPort));
    auto localImageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(
        Dss::Core::Config::instance().paths().dataRoot);
    auto trackDataStorage = std::make_shared<Dss::Storage::TrackDataStorageBackend>(
        Dss::Core::Config::instance().paths().dataRoot);
    localImageStorage->setBus(&m_bus);
    trackDataStorage->setBus(&m_bus);

    auto imageProcessor = std::make_shared<Dss::Processing::ImageProcessor>(m_bus);
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    replaySource->setFrameCallback(
        [imageProcessor, localImageStorage](Dss::Processing::FramePacket packet) {
            if (localImageStorage->isRunning() && !packet.rawImage.empty()) {
                Dss::Storage::RawImageMetadata metadata{};
                metadata.width = packet.width;
                metadata.height = packet.height;
                metadata.exposure = packet.metadata;
                metadata.exposureTimeMilliseconds =
                    static_cast<double>(packet.metadata.exposureTime) * 1000.0;
                metadata.frameFrequency = packet.metadata.frameFrequency;
                (void)localImageStorage->enqueueSessionFrame(packet.frameSeq, metadata,
                                                             packet.rawImage);
            }
            (void)imageProcessor->submitFrame(std::move(packet));
        });
    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [trackDataStorage](const Dss::Core::TrackResultEvent& event) {
            if (trackDataStorage->isRunning()) {
                (void)trackDataStorage->enqueueTrackResult(event);
            }
        }));

    m_registry.registerService<Dss::Processing::ImageProcessor>("image_processor", imageProcessor);
    m_registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>("replay_source",
                                                                           replaySource);
    m_registry.registerService<Dss::Acquisition::IFrameSource>("replay_source",
                                                               std::move(replaySource));
    m_registry.registerService<Dss::Storage::LocalImageStorageBackend>("image_storage",
                                                                       localImageStorage);
    m_registry.registerService<Dss::Storage::IStorageBackend>("image_storage",
                                                              std::move(localImageStorage));
    m_registry.registerService<Dss::Storage::TrackDataStorageBackend>("track_data_storage",
                                                                      trackDataStorage);
    m_registry.registerService<Dss::Storage::IStorageBackend>("track_data_storage",
                                                              std::move(trackDataStorage));
#endif
}

}  // namespace Dss::App
