#include <cassert>
#include <stdexcept>

#include "dss/core/event/event_bus.h"

namespace {

struct TestMessage {};

void messageBusContainsSubscriberFailures() {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    int handledCount = 0;
    int failureCount = 0;

    bus.setExceptionHandler([&failureCount](std::exception_ptr error) {
        ++failureCount;
        try {
            std::rethrow_exception(error);
        } catch (const std::runtime_error&) {
        }
    });

    auto failing = bus.subscribe<TestMessage>(
        [](const TestMessage&) { throw std::runtime_error("subscriber failed"); });
    auto succeeding =
        bus.subscribe<TestMessage>([&handledCount](const TestMessage&) { ++handledCount; });

    bus.emit(TestMessage{});

    assert(failureCount == 1);
    assert(handledCount == 1);
}

void messageBusContainsExceptionHandlerFailures() {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    bus.setExceptionHandler([](std::exception_ptr) { throw std::runtime_error("handler failed"); });
    auto failing = bus.subscribe<TestMessage>(
        [](const TestMessage&) { throw std::runtime_error("subscriber failed"); });

    bus.emit(TestMessage{});
}

}  // namespace

int main() {
    messageBusContainsSubscriberFailures();
    messageBusContainsExceptionHandlerFailures();
    return 0;
}
