#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>

#include "dss/ui/main_window.h"

#ifdef DSS_HAS_ELA
#include <ElaCheckBox.h>
#include <ElaComboBox.h>
#include <ElaPushButton.h>
#include <ElaSlider.h>
#endif

namespace Dss::Ui {
void MainWindow::setupControlPage() {
    m_controlPage = new QWidget;
    auto* layout = new QVBoxLayout(m_controlPage);
    auto* replay = &m_mainViewModel.replay();
    auto* display = &m_mainViewModel.display();
    auto* processing = &m_mainViewModel.processing();
    auto* tracking = &m_mainViewModel.tracking();
    auto* storage = &m_mainViewModel.storage();

    auto* sequenceRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnSelectSequence = new ElaPushButton("Select Sequence");
#else
    auto* btnSelectSequence = new QPushButton("Select Sequence");
#endif
    auto* sequenceLabel = new QLabel("Frames: 0");
    auto* currentFrameLabel = new QLabel("Current: 0/0");
    auto* replayProgress = new QSlider(Qt::Horizontal);
    replayProgress->setObjectName("replay_progress");
    replayProgress->setRange(0, 0);
    replayProgress->setMinimumWidth(220);
    sequenceRow->addWidget(btnSelectSequence);
    sequenceRow->addWidget(sequenceLabel);
    sequenceRow->addWidget(currentFrameLabel);
    sequenceRow->addWidget(replayProgress, 1);

    connect(btnSelectSequence, &QPushButton::clicked, [this, replay] {
        const auto files = QFileDialog::getOpenFileNames(
            this, "Select Image Sequence", QString(),
            "Image Sequence (*.bmp *.png *.jpg *.jpeg *.tif *.tiff *.raw);;All Files (*)");
        if (!files.isEmpty()) {
            replay->selectReplayFiles(files);
        }
    });
    connect(replay, &ReplayViewModel::replayFrameCountChanged, [sequenceLabel](int count) {
        sequenceLabel->setText(QString("Frames: %1").arg(count));
    });
    connect(replay, &ReplayViewModel::replayFrameCountChanged,
            [replay, currentFrameLabel](int count) {
                currentFrameLabel->setText(
                    QString("Current: %1/%2").arg(replay->replayCurrentFrame()).arg(count));
            });
    connect(replay, &ReplayViewModel::replayCurrentFrameChanged,
            [replay, currentFrameLabel](int frame) {
                currentFrameLabel->setText(
                    QString("Current: %1/%2").arg(frame).arg(replay->replayFrameCount()));
            });

    connect(replay, &ReplayViewModel::replayFrameCountChanged, replayProgress,
            [replayProgress](int count) { replayProgress->setRange(0, std::max(0, count - 1)); });
    connect(replay, &ReplayViewModel::replayCurrentFrameChanged, replayProgress,
            [replayProgress](int frame) { replayProgress->setValue(std::max(0, frame - 1)); });
    connect(replayProgress, &QSlider::sliderReleased,
            [replay, replayProgress] { (void)replay->seekReplayFrame(replayProgress->value()); });

    auto* grabRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnStart = new ElaPushButton("Start Replay");
    auto* btnStop = new ElaPushButton("Pause Replay");
    auto* btnStepBackward = new ElaPushButton("Step Back");
    auto* btnStepForward = new ElaPushButton("Step Forward");
#else
    auto* btnStart = new QPushButton("Start Replay");
    auto* btnStop = new QPushButton("Pause Replay");
    auto* btnStepBackward = new QPushButton("Step Back");
    auto* btnStepForward = new QPushButton("Step Forward");
#endif
    grabRow->addWidget(btnStart);
    grabRow->addWidget(btnStop);
    grabRow->addWidget(btnStepBackward);
    grabRow->addWidget(btnStepForward);

    connect(btnStart, &QPushButton::clicked, replay, &ReplayViewModel::startGrab);
    connect(btnStop, &QPushButton::clicked, replay, &ReplayViewModel::stopGrab);
    connect(btnStepBackward, &QPushButton::clicked, replay, &ReplayViewModel::stepReplayBackward);
    connect(btnStepForward, &QPushButton::clicked, replay, &ReplayViewModel::stepReplayForward);

    auto* processingRow = new QHBoxLayout;
    processingRow->addWidget(new QLabel("Processing:"));
#ifdef DSS_HAS_ELA
    auto* processingCombo = new ElaComboBox;
#else
    auto* processingCombo = new QComboBox;
#endif
    processingCombo->addItems({"None", "OpenCV", "Diff"});
    processingRow->addWidget(processingCombo);

    connect(processingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [processing](int index) {
                static constexpr int modeMap[] = {0, 3, 1};
                if (index >= 0 && index < 3) {
                    processing->setProcessingMode(modeMap[index]);
                }
            });

    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("Track Mode:"));
#ifdef DSS_HAS_ELA
    auto* modeCombo = new ElaComboBox;
#else
    auto* modeCombo = new QComboBox;
#endif
    modeCombo->addItems({"Init", "GEO", "SC", "LEO", "Manual"});
    modeRow->addWidget(modeCombo);

    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [tracking](int index) {
        static constexpr int modeMap[] = {-1, 0, 3, 4, 5};
        if (index >= 0 && index < 5) {
            tracking->setTrackMode(modeMap[index]);
        }
    });

    auto* expRow = new QHBoxLayout;
    expRow->addWidget(new QLabel("Exposure (ms):"));
#ifdef DSS_HAS_ELA
    auto* expSlider = new ElaSlider(Qt::Horizontal);
#else
    auto* expSlider = new QSlider(Qt::Horizontal);
#endif
    expSlider->setRange(1, 1000);
    expSlider->setValue(100);
    auto* expLabel = new QLabel("100");
    expRow->addWidget(expSlider);
    expRow->addWidget(expLabel);

    connect(expSlider, &QSlider::valueChanged, [this, expLabel](int val) {
        expLabel->setText(QString::number(val));
        m_mainViewModel.setExposure(val);
    });

    auto* saveRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* chkSave = new ElaCheckBox("Save Data");
#else
    auto* chkSave = new QCheckBox("Save Data");
#endif
    saveRow->addWidget(chkSave);
    connect(chkSave, &QCheckBox::toggled, [storage](bool checked) {
        if (checked) {
            storage->startSaving();
        } else {
            storage->stopSaving();
        }
    });

    layout->addLayout(sequenceRow);
    layout->addLayout(grabRow);
    auto* diagnosticsLabel = new QLabel(replay->runtimeDiagnosticsText());
    diagnosticsLabel->setObjectName("runtime_diagnostics");
    diagnosticsLabel->setMinimumWidth(520);
    connect(replay, &ReplayViewModel::runtimeDiagnosticsTextChanged, diagnosticsLabel,
            &QLabel::setText);

    layout->addLayout(processingRow);
    layout->addWidget(diagnosticsLabel);
    layout->addLayout(modeRow);
    layout->addLayout(expRow);
    layout->addLayout(saveRow);

    auto* statsLabel = new QLabel("Image: -- | FPS: --");
    statsLabel->setObjectName("stats_label");
    layout->addWidget(statsLabel);

    connect(display, &DisplayViewModel::imageStatsUpdated,
            [statsLabel](double minVal, double maxVal, double avg, double stdDev) {
                statsLabel->setText(QString("Min: %1 | Max: %2 | Avg: %3 | Std: %4")
                                        .arg(minVal, 0, 'f', 1)
                                        .arg(maxVal, 0, 'f', 1)
                                        .arg(avg, 0, 'f', 1)
                                        .arg(stdDev, 0, 'f', 1));
            });

    layout->addStretch();
}

}  // namespace Dss::Ui
