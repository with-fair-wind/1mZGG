#include "dss/ui/main_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

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
    sequenceRow->addWidget(btnSelectSequence);
    sequenceRow->addWidget(sequenceLabel);
    sequenceRow->addWidget(currentFrameLabel);

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

    auto* grabRow = new QHBoxLayout;
#ifdef DSS_HAS_ELA
    auto* btnStart = new ElaPushButton("Start Replay");
    auto* btnStop = new ElaPushButton("Pause Replay");
    auto* btnStepForward = new ElaPushButton("Step Forward");
#else
    auto* btnStart = new QPushButton("Start Replay");
    auto* btnStop = new QPushButton("Pause Replay");
    auto* btnStepForward = new QPushButton("Step Forward");
#endif
    grabRow->addWidget(btnStart);
    grabRow->addWidget(btnStop);
    grabRow->addWidget(btnStepForward);

    connect(btnStart, &QPushButton::clicked, replay, &ReplayViewModel::startGrab);
    connect(btnStop, &QPushButton::clicked, replay, &ReplayViewModel::stopGrab);
    connect(btnStepForward, &QPushButton::clicked, replay, &ReplayViewModel::stepReplayForward);

    auto* processingRow = new QHBoxLayout;
    processingRow->addWidget(new QLabel("Processing:"));
#ifdef DSS_HAS_ELA
    auto* processingCombo = new ElaComboBox;
#else
    auto* processingCombo = new QComboBox;
#endif
    processingCombo->addItems({"None", "OpenCV"});
    processingRow->addWidget(processingCombo);

    connect(processingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [processing](int index) {
                static constexpr int modeMap[] = {0, 3};
                if (index >= 0 && index < 2) {
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
    layout->addLayout(processingRow);
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