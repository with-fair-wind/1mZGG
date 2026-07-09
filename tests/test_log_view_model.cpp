#include <QCoreApplication>
#include <QEventLoop>
#include <QStringList>
#include <QThread>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/core/message_bus.h"
#include "dss/core/service_registry.h"
#include "dss/ui/view_model/log_view_model.h"
#include "dss/ui/view_model/view_model_context.h"

namespace {

using namespace std::chrono_literals;

class QCoreApplicationFixture {
public:
    QCoreApplicationFixture() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char appName[] = "test_log_view_model";
            static char* argv[] = {appName, nullptr};
            m_app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> m_app;
};

using MessageBus = Dss::Core::MessageBus;

template <typename T>
auto waitForReady(std::future<T>& future, std::chrono::milliseconds timeout = 2s) -> bool {
    auto* app = QCoreApplication::instance();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (future.wait_for(0ms) == std::future_status::ready) {
            return true;
        }
        if (app != nullptr) {
            app->processEvents(QEventLoop::AllEvents, 10);
        }
        std::this_thread::sleep_for(5ms);
    }
    return future.wait_for(0ms) == std::future_status::ready;
}

}  // namespace

TEST(LogViewModel, ForwardsCoreLogMessagesAndFiltersByMinimumLevel) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::UiServiceContext context{.bus = bus, .registry = registry};
    Dss::Ui::LogViewModel logs(context);

    std::vector<QString> appended;
    QObject::connect(&logs, &Dss::Ui::LogViewModel::logEntryAppended,
                     [&appended](const QString& text) { appended.push_back(text); });

    bus.emit(Dss::Core::LogMessageEvent{.level = Dss::Core::LogLevel::Info,
                                        .message = "[INFO] replay ready"});
    ASSERT_EQ(appended.size(), 1U);
    EXPECT_EQ(appended.front(), QString("[INFO] replay ready"));
    EXPECT_EQ(logs.visibleLogEntries(), QStringList{"[INFO] replay ready"});

    logs.setLogMinimumLevel(static_cast<int>(Dss::Core::LogLevel::Error));
    bus.emit(Dss::Core::LogMessageEvent{.level = Dss::Core::LogLevel::Warning,
                                        .message = "[WARN] hidden"});
    EXPECT_EQ(appended.size(), 1U);
    EXPECT_TRUE(logs.visibleLogEntries().isEmpty());

    bus.emit(Dss::Core::LogMessageEvent{.level = Dss::Core::LogLevel::Error,
                                        .message = "[ERROR] visible"});
    ASSERT_EQ(appended.size(), 2U);
    EXPECT_EQ(appended.back(), QString("[ERROR] visible"));
    EXPECT_EQ(logs.visibleLogEntries(), QStringList{"[ERROR] visible"});
}

TEST(LogViewModel, ConvertsNetworkAndSerialErrorsToUiLogs) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::UiServiceContext context{.bus = bus, .registry = registry};
    Dss::Ui::LogViewModel logs(context);

    std::vector<QString> appended;
    QObject::connect(&logs, &Dss::Ui::LogViewModel::logEntryAppended,
                     [&appended](const QString& text) { appended.push_back(text); });

    bus.emit(Dss::Core::NetworkTransmissionErrorEvent{
        .channel = "image_sender", .message = "closed", .attemptedBytes = 16});
    bus.emit(Dss::Core::SerialFrameErrorEvent{.channel = "display",
                                              .message = "bad tail",
                                              .expectedBytes = 20,
                                              .actualBytes = 18,
                                              .observedHeader = 0xAA,
                                              .observedTail = 0x55});
    bus.emit(Dss::Core::SerialDecodeErrorEvent{.channel = "servo",
                                               .message = "short frame",
                                               .field = "speed",
                                               .byteOffset = 4,
                                               .rawValue = 1});
    bus.emit(Dss::Core::StorageWriteErrorEvent{
        .backend = "image_storage", .path = "frame.raw", .message = "disk full"});

    ASSERT_EQ(appended.size(), 4U);
    EXPECT_TRUE(appended[0].contains("Network send failed"));
    EXPECT_TRUE(appended[1].contains("Serial frame dropped"));
    EXPECT_TRUE(appended[2].contains("Serial decode failed"));
    EXPECT_TRUE(appended[3].contains("Storage write failed"));
}

TEST(LogViewModel, ErrorEventFromWorkerThreadUpdatesOnObjectThread) {
    QCoreApplicationFixture app;
    MessageBus bus;
    Dss::Core::ServiceRegistry registry;
    Dss::Ui::UiServiceContext context{.bus = bus, .registry = registry};
    Dss::Ui::LogViewModel logs(context);

    std::promise<QThread*> signalThreadPromise;
    auto signalThreadFuture = signalThreadPromise.get_future();
    QObject::connect(&logs, &Dss::Ui::LogViewModel::logEntryAppended,
                     [&signalThreadPromise](const QString&) {
                         signalThreadPromise.set_value(QThread::currentThread());
                     });

    std::jthread worker([&bus] {
        bus.emit(Dss::Core::StorageWriteErrorEvent{
            .backend = "image_storage", .path = "frame.raw", .message = "disk full"});
    });
    worker.join();

    ASSERT_TRUE(waitForReady(signalThreadFuture));
    EXPECT_EQ(signalThreadFuture.get(), logs.thread());
}
