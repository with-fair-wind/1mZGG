#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dss/core/diagnostics/emergency_report.h"
#include "dss/core/event/event_bus.h"

namespace {

struct TestMessage {};

void messageBusContainsSubscriberFailures() {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    int handledCount = 0;
    int failureCount = 0;
    bool matchingFailure = false;

    bus.setExceptionHandler([&failureCount, &matchingFailure](std::exception_ptr error) {
        ++failureCount;
        try {
            std::rethrow_exception(error);
        } catch (const std::runtime_error& caught) {
            matchingFailure = std::string_view(caught.what()) == "subscriber failed";
        }
    });

    auto failing = bus.subscribe<TestMessage>(
        [](const TestMessage&) { throw std::runtime_error("subscriber failed"); });
    auto succeeding =
        bus.subscribe<TestMessage>([&handledCount](const TestMessage&) { ++handledCount; });

    bus.emit(TestMessage{});

    if (failureCount != 1 || handledCount != 1 || !matchingFailure) {
        throw std::runtime_error("message bus failed to contain subscriber failure");
    }
}

void messageBusContainsExceptionHandlerFailures() {
    Dss::Evt::BasicMessageBus<Dss::Evt::SharedMutexLock> bus;
    bus.setExceptionHandler([](std::exception_ptr) { throw std::runtime_error("handler failed"); });
    auto failing = bus.subscribe<TestMessage>(
        [](const TestMessage&) { throw std::runtime_error("subscriber failed"); });

    bus.emit(TestMessage{});
}

void eventPreservesReferenceCategoryAndValueCopies() {
    Dss::Evt::Event<int(std::string&&)> referenceEvent;
    auto referenceConnection = referenceEvent.subscribe([](std::string&& value) {
        const std::string consumed = std::move(value);
        return static_cast<int>(consumed.size());
    });
    if (referenceEvent.emit(std::string{"payload"}) != std::vector<int>{7}) {
        throw std::runtime_error("rvalue-reference event lost its argument");
    }

    Dss::Evt::Event<std::string(std::string)> valueEvent;
    auto first = valueEvent.subscribe([](std::string value) { return value; });
    auto second = valueEvent.subscribe([](std::string value) { return value; });
    const auto values = valueEvent.emit(std::string{"payload"});
    if (values != std::vector<std::string>{"payload", "payload"}) {
        throw std::runtime_error("by-value multicast arguments must remain independent");
    }
}

}  // namespace

int main() try {
    messageBusContainsSubscriberFailures();
    messageBusContainsExceptionHandlerFailures();
    eventPreservesReferenceCategoryAndValueCopies();
    return 0;
} catch (const std::exception& error) {
    Dss::Core::reportEmergencyFailure(error.what());
    Dss::Core::reportEmergencyFailure("\n");
    return 1;
} catch (...) {
    Dss::Core::reportEmergencyFailure("DSS: unhandled entry-point failure\n");
    return 1;
}
