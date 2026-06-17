# ViewModel Modularization Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract UI logging from the monolithic `Dss::Ui::ViewModel` into a dedicated `LogViewModel` while keeping existing UI behavior and compatibility signals intact.

**Architecture:** Add a lightweight `UiServiceContext` to share `MessageBus` and `ServiceRegistry` references across future child ViewModels. Implement `LogViewModel` as a focused `QObject` that subscribes to log-related core events, owns log filtering/cache state, and emits the same log signals currently exposed by `ViewModel`. Keep old `ViewModel` APIs as forwarding methods for compatibility, then move the Logs page to use `LogViewModel` directly.

**Tech Stack:** C++23, Qt QObject signals/slots, GoogleTest, CMake/CTest, existing `dss_ui_qt` target.

---

### Task 1: Add Logging Module Test

**Files:**
- Create: `tests/test_log_view_model.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/test_log_view_model.cpp` with these checks:

```cpp
#include <QCoreApplication>
#include <QStringList>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "dss/core/events.h"
#include "dss/core/service_registry.h"
#include "dss/ui/log_view_model.h"
#include "dss/ui/view_model_context.h"

namespace {

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

}  // namespace

TEST(LogViewModel, ForwardsCoreLogMessagesAndFiltersByMinimumLevel) {
    QCoreApplicationFixture app;
    Dss::Core::MessageBus bus;
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
    Dss::Core::MessageBus bus;
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

    ASSERT_EQ(appended.size(), 3U);
    EXPECT_TRUE(appended[0].contains("Network send failed"));
    EXPECT_TRUE(appended[1].contains("Serial frame dropped"));
    EXPECT_TRUE(appended[2].contains("Serial decode failed"));
}
```

Add the test to the `if(TARGET dss_ui_qt)` block in `tests/CMakeLists.txt`:

```cmake
dss_add_qt_test(test_log_view_model
    test_log_view_model.cpp
    dss_ui_qt
)
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
cmake --build --preset clang-cl-debug --target test_log_view_model
```

Expected: build fails because `dss/ui/log_view_model.h` and `dss/ui/view_model_context.h` do not exist.

### Task 2: Implement UiServiceContext and LogViewModel

**Files:**
- Create: `include/dss/ui/view_model_context.h`
- Create: `include/dss/ui/log_view_model.h`
- Create: `src/ui/log_view_model.cpp`

- [ ] **Step 1: Add shared context**

Create `include/dss/ui/view_model_context.h`:

```cpp
#pragma once

#include "dss/core/events.h"
#include "dss/core/service_registry.h"

namespace Dss::Ui {

/**
 * @brief UI 子 ViewModel 共享的服务上下文。
 */
struct UiServiceContext {
    Dss::Core::MessageBus& bus;            ///< 应用事件总线。
    Dss::Core::ServiceRegistry& registry;  ///< 应用服务注册表。
};

}  // namespace Dss::Ui
```

- [ ] **Step 2: Add LogViewModel declaration**

Create `include/dss/ui/log_view_model.h` with a `QObject` class exposing `logMinimumLevel()`、`visibleLogEntries()`、`setLogMinimumLevel()` and signals `logEntryAppended`、`logEntriesChanged`、`logMinimumLevelChanged`、`statusTextChanged`.

- [ ] **Step 3: Add LogViewModel implementation**

Move the existing formatting logic from `src/ui/view_model.cpp` into `src/ui/log_view_model.cpp`: `makeNetworkTransmissionErrorStatus`、`makeNetworkTransmissionErrorLog`、`makeSerialFrameErrorLog`、`makeSerialDecodeErrorLog`、`logLevelFromInt`、`appendLogEntry`、`isLogLevelVisible` and event subscription setup.

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
cmake --build --preset clang-cl-debug --target test_log_view_model
ctest --test-dir build\clang-cl-debug -R test_log_view_model --output-on-failure
```

Expected: `test_log_view_model` passes.

### Task 3: Wire ViewModel Compatibility Facade

**Files:**
- Modify: `include/dss/ui/view_model.h`
- Modify: `src/ui/view_model.cpp`
- Test: `tests/test_view_model_network.cpp`

- [ ] **Step 1: Add compatibility test**

Keep existing `ViewModelNetwork.ForwardsCoreLogMessagesToUiLogSignal`、`FiltersUiLogEntriesByMinimumLevel`、`KeepsOnlyRecentUiLogEntries`、`ForwardsSerialFrameErrorsToUiLogSignal` and `ForwardsSerialDecodeErrorsToUiLogSignal` unchanged. They must still pass through the old `ViewModel` API.

- [ ] **Step 2: Add LogViewModel ownership**

Add `#include "dss/ui/log_view_model.h"` to `view_model.h`, add a member:

```cpp
LogViewModel m_logs;  ///< 日志子 ViewModel，负责日志缓存、过滤与事件桥接。
```

Construct it in `ViewModel::ViewModel` with `UiServiceContext{.bus = bus, .registry = registry}`.

- [ ] **Step 3: Forward existing APIs and signals**

Forward:

```cpp
int ViewModel::logMinimumLevel() const { return m_logs.logMinimumLevel(); }
QStringList ViewModel::visibleLogEntries() const { return m_logs.visibleLogEntries(); }
void ViewModel::setLogMinimumLevel(int level) { m_logs.setLogMinimumLevel(level); }
LogViewModel& ViewModel::logs() { return m_logs; }
const LogViewModel& ViewModel::logs() const { return m_logs; }
```

Connect child signals to old signals in the constructor:

```cpp
connect(&m_logs, &LogViewModel::logEntryAppended, this, &ViewModel::logEntryAppended);
connect(&m_logs, &LogViewModel::logEntriesChanged, this, &ViewModel::logEntriesChanged);
connect(&m_logs, &LogViewModel::logMinimumLevelChanged, this, &ViewModel::logMinimumLevelChanged);
connect(&m_logs, &LogViewModel::statusTextChanged, this, &ViewModel::statusTextChanged);
```

Remove the old log event subscriptions from `ViewModel::setupSubscriptions()`.

- [ ] **Step 4: Run compatibility tests**

Run:

```powershell
cmake --build --preset clang-cl-debug --target test_view_model_network
ctest --test-dir build\clang-cl-debug -R test_view_model_network --output-on-failure
```

Expected: existing `ViewModel` log tests still pass.

### Task 4: Move MainWindow Logs Page to LogViewModel

**Files:**
- Modify: `src/ui/main_window.cpp`
- Test: `tests/test_main_window_layout.cpp`

- [ ] **Step 1: Switch log page bindings**

In `MainWindow::setupLogPage`, bind log controls to `m_vm.logs()` instead of `m_vm`:

```cpp
auto& logs = m_vm.logs();
levelCombo->setCurrentIndex(logs.logMinimumLevel());
connect(&logs, &LogViewModel::logEntriesChanged, logText, refreshLogText);
connect(levelCombo, &QComboBox::currentIndexChanged, &logs, &LogViewModel::setLogMinimumLevel);
```

- [ ] **Step 2: Run UI layout test**

Run:

```powershell
cmake --build --preset clang-cl-debug --target test_main_window_layout
ctest --test-dir build\clang-cl-debug -R test_main_window_layout --output-on-failure
```

Expected: layout test passes.

### Task 5: Final Verification

**Files:**
- Check all files touched in Tasks 1-4.

- [ ] **Step 1: Run focused tests**

Run:

```powershell
cmake --build --preset clang-cl-debug --target test_log_view_model test_view_model_network test_main_window_layout
ctest --test-dir build\clang-cl-debug -R "test_log_view_model|test_view_model_network|test_main_window_layout" --output-on-failure
```

Expected: all focused tests pass.

- [ ] **Step 2: Run formatting and comment checks**

Run:

```powershell
rg -n "\* \*|@return\s*$|@param\s*$" include src tests
git diff --check
```

Expected: no broken Doxygen patterns; `git diff --check` reports no whitespace errors other than possible CRLF/LF conversion warnings.
