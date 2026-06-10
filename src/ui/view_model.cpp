#include "dss/ui/view_model.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "dss/acquisition/i_frame_source.h"
#include "dss/acquisition/image_sequence_frame_source.h"
#include "dss/core/config.h"
#include "dss/processing/image_processor.h"
#ifdef DSS_HAS_OPENCV
#include "dss/processing/opencv_processing_strategy.h"
#endif
#include "dss/storage/local_image_storage_backend.h"
#include "dss/storage/track_data_storage_backend.h"
#include "dss/tracking/manual_tracker.h"
#include "dss/tracking/track_manager.h"

namespace Dss::Ui {

ViewModel::ViewModel(MessageBus& bus, Dss::Core::ServiceRegistry& registry, QObject* parent)
    : QObject(parent), m_bus(bus), m_registry(registry) {
    setupSubscriptions();
}

ViewModel::~ViewModel() = default;

/**
 * @brief 加载回放文件列表并初始化序列源
 * @param files 图像文件路径列表
 * @return 成功返回 true
 */
bool ViewModel::selectReplayFiles(const QStringList& files) {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        setStatus("Replay source is not registered");
        return false;
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(files.size()));
    for (const auto& file : files) {
        if (!file.isEmpty()) {
            paths.emplace_back(file.toStdWString());
        }
    }

    auto result = replaySource->setFiles(std::move(paths));
    if (!result.has_value()) {
        m_replayFrameCount = 0;
        setReplayCurrentFrame(0);
        Q_EMIT replayFrameCountChanged(m_replayFrameCount);
        setStatus(QString::fromStdString(result.error()));
        return false;
    }

    auto initResult = replaySource->init();
    if (!initResult.has_value()) {
        m_replayFrameCount = 0;
        setReplayCurrentFrame(0);
        Q_EMIT replayFrameCountChanged(m_replayFrameCount);
        setStatus(QString::fromStdString(initResult.error()));
        return false;
    }

    m_replayFrameCount = static_cast<int>(replaySource->frameCount());
    setReplayCurrentFrame(0);
    Q_EMIT replayFrameCountChanged(m_replayFrameCount);
    setStatus(QString("Sequence selected: %1 frames").arg(m_replayFrameCount));
    return true;
}

/// 启动帧源与图像处理器，进入回放状态
void ViewModel::startGrab() {
    if (m_grabbing) {
        return;
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (!processor || !frameSource) {
        setStatus("Replay services are not registered");
        return;
    }

    auto initResult = frameSource->init();
    if (!initResult.has_value()) {
        setStatus(QString::fromStdString(initResult.error()));
        return;
    }

    setReplayCurrentFrame(0);
    processor->start();
    frameSource->start();
    m_grabbing = true;
    m_statusText = "Replaying...";
    Q_EMIT grabbingChanged(true);
    Q_EMIT statusTextChanged(m_statusText);
    m_bus.emit(Dss::Core::GrabStartedEvent{frameSource->frameWidth(), frameSource->frameHeight()});
}

/// 停止帧源与图像处理器，退出回放状态
void ViewModel::stopGrab() {
    auto frameSource = m_registry.tryGet<Dss::Acquisition::IFrameSource>("replay_source");
    if (frameSource) {
        frameSource->stop();
    }

    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (processor) {
        processor->stop();
    }

    m_grabbing = false;
    m_statusText = "Stopped";
    Q_EMIT grabbingChanged(false);
    Q_EMIT statusTextChanged(m_statusText);
    m_bus.emit(Dss::Core::GrabStoppedEvent{});
}

/// 单步前进一帧；若正在连续回放则先停止
bool ViewModel::stepReplayForward() {
    if (m_grabbing) {
        stopGrab();
    }

    auto replaySource =
        m_registry.tryGet<Dss::Acquisition::ImageSequenceFrameSource>("replay_source");
    if (!replaySource) {
        setStatus("Replay source is not registered");
        return false;
    }

    auto result = replaySource->stepForward();
    if (!result.has_value()) {
        setStatus(QString::fromStdString(result.error()));
        return false;
    }
    return true;
}

void ViewModel::setProcessingMode(int mode) {
    if (m_processingMode != mode) {
        m_processingMode = mode;
        Q_EMIT processingModeChanged(mode);
    }
    configureProcessingStrategy();
}

void ViewModel::setTrackMode(int mode) {
    if (m_trackMode != mode) {
        m_trackMode = mode;
        Q_EMIT trackModeChanged(mode);
        configureTrackingStrategy();
    }
}

void ViewModel::setExposure(double ms) {
    if (m_exposure != ms) {
        m_exposure = ms;
        Q_EMIT exposureChanged(ms);
    }
}

/// 记录手动目标并切换/刷新手动跟踪策略
void ViewModel::selectTarget(QPointF pos) {
    m_manualTarget = makeManualTarget(pos);
    m_bus.emit(Dss::Core::ManualTargetSelectEvent{static_cast<float>(pos.x()),
                                                  static_cast<float>(pos.y())});
    if (m_trackMode != static_cast<int>(Dss::Core::TrackMode::Manual)) {
        setTrackMode(static_cast<int>(Dss::Core::TrackMode::Manual));
    } else {
        configureTrackingStrategy();
    }
    setStatus(
        QString("Manual target selected: %1, %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
}

/**
 * @brief 启动图像与跟踪数据本地存储
 * 若存储后端未就绪则先初始化；任一环节失败则回滚已启动的后端
 */
void ViewModel::startSaving() {
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
    if (!storage) {
        setStatus("Image storage is not registered");
        return;
    }
    if (!storage->isReady()) {
        auto initResult = storage->init(storage->baseDir());
        if (!initResult.has_value()) {
            setStatus(QString::fromStdString(initResult.error()));
            return;
        }
    }
    auto startResult = storage->start();
    if (!startResult.has_value()) {
        setStatus(QString::fromStdString(startResult.error()));
        return;
    }

    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage) {
        if (!trackStorage->isReady()) {
            auto initTrackResult = trackStorage->init(trackStorage->baseDir());
            if (!initTrackResult.has_value()) {
                storage->stop();
                setStatus(QString::fromStdString(initTrackResult.error()));
                return;
            }
        }
        auto startTrackResult = trackStorage->start();
        if (!startTrackResult.has_value()) {
            storage->stop();
            setStatus(QString::fromStdString(startTrackResult.error()));
            return;
        }
    }

    m_saving = true;
    Q_EMIT savingChanged(true);
    setStatus("Saving enabled");
}

/// 停止图像与跟踪数据存储
void ViewModel::stopSaving() {
    auto storage = m_registry.tryGet<Dss::Storage::LocalImageStorageBackend>("image_storage");
    if (storage) {
        storage->stop();
    }
    auto trackStorage =
        m_registry.tryGet<Dss::Storage::TrackDataStorageBackend>("track_data_storage");
    if (trackStorage) {
        trackStorage->stop();
    }
    m_saving = false;
    Q_EMIT savingChanged(false);
    setStatus("Saving stopped");
}

void ViewModel::toggleZoom(int level) {
    m_bus.emit(Dss::Core::ZoomChangeEvent{level});
}

/// 订阅显示刷新、处理完成、跟踪结果与主控事件
void ViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::DisplayRefreshEvent>(
        [this](const Dss::Core::DisplayRefreshEvent& e) { onDisplayRefresh(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::ProcessingCompleteEvent>(
        [this](const Dss::Core::ProcessingCompleteEvent& e) { onProcessingComplete(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::TrackResultEvent>(
        [this](const Dss::Core::TrackResultEvent& e) { onTrackResult(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::MasterControlEvent>(
        [this](const Dss::Core::MasterControlEvent& e) { onMasterControl(e); }));
}

/**
 * @brief 将 DisplayRefreshEvent 中的灰度缓冲转为 QImage 并通知 UI
 * @param event 显示刷新事件
 */
void ViewModel::onDisplayRefresh(const Dss::Core::DisplayRefreshEvent& event) {
    if (!event.displayImage || event.width == 0 || event.height == 0 || event.stride == 0) {
        return;
    }

    const auto expectedSize = static_cast<size_t>(event.stride) * static_cast<size_t>(event.height);
    if (event.displayImage->size() < expectedSize) {
        return;
    }

    QImage image(event.displayImage->data(), static_cast<int>(event.width),
                 static_cast<int>(event.height), static_cast<qsizetype>(event.stride),
                 QImage::Format_Grayscale8);
    setReplayCurrentFrame(static_cast<int>(event.frameSeq) + 1);
    Q_EMIT displayImageReady(image.copy());
}

/// 转发图像统计量至 UI
void ViewModel::onProcessingComplete(const Dss::Core::ProcessingCompleteEvent& event) {
    const auto& s = event.stats;
    Q_EMIT imageStatsUpdated(s.minVal, s.maxVal, s.avg, s.stdDev);
}

/// 更新目标数量与首个目标的跟踪信息
void ViewModel::onTrackResult(const Dss::Core::TrackResultEvent& event) {
    Q_EMIT targetListUpdated(static_cast<int>(event.targets.size()));

    if (!event.targets.empty()) {
        const auto& t = event.targets.front();
        auto info = QString("Target: %1 | AZ: %2 EL: %3")
                        .arg(QString::fromStdString(t.targetId))
                        .arg(static_cast<double>(t.predictedPosAe.x), 0, 'f', 4)
                        .arg(static_cast<double>(t.predictedPosAe.y), 0, 'f', 4);
        Q_EMIT trackInfoUpdated(info);
    }
}

/**
 * @brief 响应外部主控指令，同步曝光、跟踪模式、保存与采集状态
 * @param event 主控事件
 */
void ViewModel::onMasterControl(const Dss::Core::MasterControlEvent& event) {
    setExposure(event.exposure);
    setTrackMode(event.trackMode);

    if (event.save && !m_saving) {
        startSaving();
    } else if (!event.save && m_saving) {
        stopSaving();
    }

    if (event.grab && !m_grabbing) {
        startGrab();
    } else if (!event.grab && m_grabbing) {
        stopGrab();
    }
}

/// 按 ProcessingMode 为 ImageProcessor 绑定或清除处理策略
void ViewModel::configureProcessingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        setStatus("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::ProcessingMode>(m_processingMode);
    switch (mode) {
        case Dss::Core::ProcessingMode::None:
            processor->setProcessingStrategy(nullptr);
            setStatus("Processing disabled");
            break;
        case Dss::Core::ProcessingMode::Direct:
#ifdef DSS_HAS_OPENCV
            processor->setProcessingStrategy(
                std::make_unique<Dss::Processing::OpenCvProcessingStrategy>());
            setStatus("OpenCV processing enabled");
#else
            processor->setProcessingStrategy(nullptr);
            setStatus("OpenCV processing is not available");
#endif
            break;
        case Dss::Core::ProcessingMode::Diff:
            processor->setProcessingStrategy(nullptr);
            setStatus("Processing mode is not available");
            break;
    }
}

/// 按 TrackMode 创建跟踪策略；手动模式时注入已选目标
void ViewModel::configureTrackingStrategy() {
    auto processor = m_registry.tryGet<Dss::Processing::ImageProcessor>("image_processor");
    if (!processor) {
        setStatus("Image processor is not registered");
        return;
    }

    const auto mode = static_cast<Dss::Core::TrackMode>(m_trackMode);
    auto strategy =
        Dss::Tracking::makeTrackingStrategy(mode, Dss::Core::Config::instance().trackingSettings());
    if (!strategy) {
        processor->setTrackingStrategy(nullptr);
        setStatus("Tracking disabled");
        return;
    }

    if (auto* manualTracker = dynamic_cast<Dss::Tracking::ManualTracker*>(strategy.get())) {
        if (m_manualTarget.has_value()) {
            manualTracker->setManualTarget(*m_manualTarget);
        }
        processor->setTrackingStrategy(std::move(strategy));
        setStatus(m_manualTarget.has_value() ? "Manual tracking target armed"
                                             : "Manual tracking enabled");
        return;
    }

    processor->setTrackingStrategy(std::move(strategy));
    setStatus("Tracking mode enabled");
}

/// 以点击位置为中心构造固定尺寸的 MeasuredBlob
auto ViewModel::makeManualTarget(QPointF pos) -> Dss::Core::MeasuredBlob {
    Dss::Core::MeasuredBlob blob{};
    blob.id = "manual";
    blob.centroid = Dss::Core::Vec2f{static_cast<float>(pos.x()), static_cast<float>(pos.y())};
    blob.minX = blob.centroid.x - 10.0F;
    blob.maxX = blob.centroid.x + 10.0F;
    blob.minY = blob.centroid.y - 10.0F;
    blob.maxY = blob.centroid.y + 10.0F;
    blob.area = 100.0F;
    blob.dn = 10000.0F;
    return blob;
}

void ViewModel::setReplayCurrentFrame(int frame) {
    if (m_replayCurrentFrame == frame) {
        return;
    }
    m_replayCurrentFrame = frame;
    Q_EMIT replayCurrentFrameChanged(frame);
}

void ViewModel::setStatus(QString text) {
    if (m_statusText == text) {
        return;
    }
    m_statusText = std::move(text);
    Q_EMIT statusTextChanged(m_statusText);
}

}  // namespace Dss::Ui
