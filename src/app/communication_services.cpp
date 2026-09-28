#include "dss/app/application_context.h"

#ifdef DSS_BUILD_APP
#include <memory>

#include "dss/acquisition/camera/i_camera_controller.h"
#include "dss/acquisition/source/frame_source_coordinator.h"
#include "dss/acquisition/source/i_frame_source.h"
#include "dss/acquisition/source/image_sequence_frame_source.h"
#ifdef DSS_HAS_SAPERA
#include "dss/acquisition/source/sapera_frame_source.h"
#endif
#include "dss/app/replay_session.h"
#include "dss/app/runtime_diagnostics.h"
#include "dss/app/service_keys.h"
#include "dss/app/track_result_data_exchange_bridge.h"
#include "dss/comm/channel/display_channel.h"
#include "dss/comm/channel/exposure_channel.h"
#include "dss/comm/channel/master_control_channel.h"
#include "dss/comm/channel/servo_channel.h"
#include "dss/comm/port/i_serial_channel.h"
#include "dss/comm/port/serial_command_interfaces.h"
#include "dss/network/endpoint/atmos_receiver.h"
#include "dss/network/endpoint/data_exchange.h"
#include "dss/network/endpoint/error_diagnostics.h"
#include "dss/network/endpoint/heartbeat.h"
#include "dss/network/endpoint/image_sender.h"
#include "dss/network/transport/i_network_channel.h"
#include "dss/processing/pipeline/image_processor.h"
#include "dss/storage/backend/local_image_storage_backend.h"
#include "dss/storage/backend/track_data_storage_backend.h"
#endif

namespace Dss::App {

void ApplicationContext::registerCommunicationServices() {
#ifdef DSS_BUILD_APP
    auto display = std::make_shared<Dss::Comm::DisplayChannel>(m_bus);
    auto exposure = std::make_shared<Dss::Comm::ExposureChannel>(m_bus);
    auto masterControl = std::make_shared<Dss::Comm::MasterControlChannel>(m_bus);
    auto servo = std::make_shared<Dss::Comm::ServoChannel>(m_bus);

    m_registry.registerService<Dss::Comm::ISerialChannel>(ServiceKey::display, display);
    m_registry.registerService<Dss::Comm::ISerialChannel>(ServiceKey::exposure, exposure);
    m_registry.registerService<Dss::Comm::IExposureCommandPort>(ServiceKey::exposure, exposure);
    m_registry.registerService<Dss::Comm::ISerialChannel>(ServiceKey::masterControl, masterControl);
    m_registry.registerService<Dss::Comm::IMasterControlStatusPort>(ServiceKey::masterControl,
                                                                    masterControl);
    m_registry.registerService<Dss::Comm::ISerialChannel>(ServiceKey::servo, servo);
    m_registry.registerService<Dss::Comm::IServoCorrectionPort>(ServiceKey::servo, servo);

    auto imageSender = std::make_shared<Dss::Network::ImageSender>(m_bus);
    auto heartbeat = std::make_shared<Dss::Network::Heartbeat>(m_bus);
    auto errorDiagnostics = std::make_shared<Dss::Network::ErrorDiagnostics>(m_bus);
    auto atmosReceiver = std::make_shared<Dss::Network::AtmosReceiver>(m_bus);

    m_registry.registerService<Dss::Network::ImageSender>(ServiceKey::imageSender, imageSender);
    m_registry.registerService<Dss::Network::INetworkChannel>(ServiceKey::imageSender, imageSender);
    m_connections.push_back(m_bus.subscribe<Dss::Core::ImageReadyForSendEvent>(
        [imageSender](const Dss::Core::ImageReadyForSendEvent& event) {
            if (!imageSender->isOpen()) {
                return;
            }
            // 仅更新单槽;imageFactory 由 ImageSender 工作线程按需调用,
            // 避免在 ImageProcessor 处理线程执行整图拉伸
            imageSender->submitForSend(event.frameSeq, event.image, event.imageFactory, event.width,
                                       event.height, event.retainedSourceBytes);
        }));
    m_registry.registerService<Dss::Network::Heartbeat>(ServiceKey::heartbeat, heartbeat);
    m_registry.registerService<Dss::Network::INetworkChannel>(ServiceKey::heartbeat, heartbeat);
    m_registry.registerService<Dss::Network::ErrorDiagnostics>(ServiceKey::errorDiagnostics,
                                                               errorDiagnostics);
    m_registry.registerService<Dss::Network::INetworkChannel>(ServiceKey::errorDiagnostics,
                                                              errorDiagnostics);
    auto dataExchange = std::make_shared<Dss::Network::DataExchange>(m_bus);
    m_registry.registerService<Dss::Network::DataExchange>(ServiceKey::dataExchange, dataExchange);
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
        ServiceKey::trackResultDataExchangeBridge, std::move(trackResultDataExchangeBridge));
    m_registry.registerService<Dss::Network::AtmosReceiver>(ServiceKey::atmosReceiver,
                                                            atmosReceiver);
    m_registry.registerService<Dss::Network::INetworkChannel>(ServiceKey::atmosReceiver,
                                                              atmosReceiver);
    m_registry.registerService<Dss::Acquisition::ICameraController>(
        ServiceKey::camera, std::make_shared<Dss::Acquisition::CommandOnlyCameraController>(
                                Dss::Core::Config::instance().commNet().cameraPort));
    auto localImageStorage = std::make_shared<Dss::Storage::LocalImageStorageBackend>(
        Dss::Core::Config::instance().paths().dataRoot);
    auto trackDataStorage = std::make_shared<Dss::Storage::TrackDataStorageBackend>(
        Dss::Core::Config::instance().paths().dataRoot);
    localImageStorage->setBus(&m_bus);
    trackDataStorage->setBus(&m_bus);

    auto imageProcessor = std::make_shared<Dss::Processing::ImageProcessor>(m_bus);
    auto replaySource = std::make_shared<Dss::Acquisition::ImageSequenceFrameSource>();
    auto frameSource = std::make_shared<Dss::Acquisition::FrameSourceCoordinator>();
    (void)frameSource->registerSource(Dss::Acquisition::FrameSourceMode::Replay, replaySource);
#ifdef DSS_HAS_SAPERA
    auto liveSource = std::make_shared<Dss::Acquisition::SaperaFrameSource>(
        Dss::Acquisition::SaperaConfig{
            .ccfPath = Dss::Core::Config::instance().paths().ccfFile.string(),
        },
        nullptr, &m_bus);
    (void)frameSource->registerSource(Dss::Acquisition::FrameSourceMode::Live, liveSource);
    m_registry.registerService<Dss::Acquisition::IFrameSource>(ServiceKey::saperaSource,
                                                               liveSource);
#endif
    frameSource->setFrameCallback(
        [imageProcessor, localImageStorage](Dss::Processing::FramePacket packet,
                                            Dss::Acquisition::FrameDeliveryContext context) {
            const auto storageGeneration = localImageStorage->sessionGeneration();
            if (localImageStorage->isRunning() && packet.rawImage && !packet.rawImage->empty()) {
                Dss::Storage::RawImageMetadata metadata{};
                metadata.width = packet.width;
                metadata.height = packet.height;
                metadata.exposure = packet.metadata;
                metadata.exposureTimeMilliseconds =
                    static_cast<double>(packet.metadata.exposureTime) * 1000.0;
                metadata.frameFrequency = packet.metadata.frameFrequency;
                (void)localImageStorage->enqueueSessionFrame(packet.frameSeq, metadata,
                                                             packet.rawImage, storageGeneration);
            }
            if (context.policy == Dss::Acquisition::FrameDeliveryPolicy::Lossless) {
                return imageProcessor->submitFrameBlocking(std::move(packet), context.stopToken);
            }
            return imageProcessor->submitFrame(std::move(packet));
        });
    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [trackDataStorage](const Dss::Core::TrackResultEvent& event) {
            const auto generation = trackDataStorage->sessionGeneration();
            if (trackDataStorage->isRunning()) {
                (void)trackDataStorage->enqueueTrackResult(event, generation);
            }
        }));

    auto runtimeDiagnostics = std::make_shared<Dss::App::RuntimeDiagnostics>(
        m_bus,
        Dss::App::RuntimeDiagnosticsSources{
            .processingDroppedFrames = [imageProcessor] { return imageProcessor->droppedFrames(); },
            .imageSuccessfulWrites =
                [localImageStorage] { return localImageStorage->successfulWrites(); },
            .imageFailedWrites = [localImageStorage] { return localImageStorage->failedWrites(); },
            .imageDroppedRequests =
                [localImageStorage] { return localImageStorage->droppedRequests(); },
            .trackSuccessfulWrites =
                [trackDataStorage] { return trackDataStorage->successfulWrites(); },
            .trackFailedWrites = [trackDataStorage] { return trackDataStorage->failedWrites(); },
            .trackDroppedRequests =
                [trackDataStorage] { return trackDataStorage->droppedRequests(); },
            .processing = [imageProcessor] { return imageProcessor->resourceSnapshot(); },
            .imageStorage = [localImageStorage] { return localImageStorage->resourceSnapshot(); },
            .trackStorage = [trackDataStorage] { return trackDataStorage->resourceSnapshot(); },
            .imageSender = [imageSender] { return imageSender->resourceSnapshot(); },
        });
    m_registry.registerService<Dss::App::RuntimeDiagnostics>(ServiceKey::runtimeDiagnostics,
                                                             runtimeDiagnostics);
    m_registry.registerService<Dss::Processing::ImageProcessor>(ServiceKey::imageProcessor,
                                                                imageProcessor);
    m_registry.registerService<Dss::Acquisition::FrameSourceCoordinator>(ServiceKey::frameSource,
                                                                         frameSource);
    m_registry.registerService<Dss::Acquisition::IFrameSource>(ServiceKey::frameSource,
                                                               frameSource);
    m_registry.registerService<Dss::Acquisition::ImageSequenceFrameSource>(ServiceKey::replaySource,
                                                                           replaySource);
    m_registry.registerService<Dss::Acquisition::IFrameSource>(ServiceKey::replaySource,
                                                               replaySource);
    m_registry.registerService<Dss::Storage::LocalImageStorageBackend>(ServiceKey::imageStorage,
                                                                       localImageStorage);
    auto replaySession =
        std::make_shared<ReplaySession>(m_bus, replaySource, imageProcessor, frameSource);
    m_registry.registerService<ReplaySession>(ServiceKey::replaySession, replaySession);
    m_stopServices = [display, exposure, masterControl, servo, atmosReceiver, frameSource,
                      replaySource, imageProcessor, localImageStorage, trackDataStorage,
                      imageSender, heartbeat, errorDiagnostics, dataExchange, replaySession] {
        // 先关闭外部输入；所有订阅者此时仍由 registry 持有。
        masterControl->close();
        exposure->close();
        display->close();
        servo->close();
        atmosReceiver->close();
        replaySession->shutdown();
        frameSource->stop();
        replaySource->stop();  // 自然结束后也 join，不能只检查 isRunning。
        imageProcessor->stop();
        localImageStorage->stop();
        trackDataStorage->stop();
        imageSender->close();
        heartbeat->close();
        errorDiagnostics->close();
        dataExchange->close();
    };
    m_registry.registerService<Dss::Storage::IStorageBackend>(ServiceKey::imageStorage,
                                                              std::move(localImageStorage));
    m_registry.registerService<Dss::Storage::TrackDataStorageBackend>(ServiceKey::trackDataStorage,
                                                                      trackDataStorage);
    m_registry.registerService<Dss::Storage::IStorageBackend>(ServiceKey::trackDataStorage,
                                                              std::move(trackDataStorage));
#endif
}

}  // namespace Dss::App
