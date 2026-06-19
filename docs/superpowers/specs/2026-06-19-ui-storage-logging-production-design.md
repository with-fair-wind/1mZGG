# UI Split, Storage Backpressure, and File Logging Design

## Goal

Reduce `MainWindow` file size without changing UI behavior, then add the first production-grade storage and logging safeguards needed for high-frame-rate operation.

## Scope

This slice covers three tightly bounded changes:

1. Split `src/ui/main_window.cpp` page-building code into focused implementation files while keeping the `MainWindow` public interface and runtime behavior unchanged.
2. Add bounded queue/backpressure reporting to local image and track-data storage workers.
3. Add optional file logging to the existing logger while preserving event-bus log delivery for the UI log page.

The slice does not add new UI pages, change visual layout, implement BMP/IFM/IMI/GAE session formats, add log search/export, or complete CUDA/Sapera integration.

## UI Architecture

`MainWindow` remains the Qt widget owner and signal wiring point. Page construction is moved into separate `.cpp` files grouped by responsibility:

- `main_window_control_page.cpp`: replay, processing, tracking, exposure, save controls.
- `main_window_display_page.cpp`: image display and stretch controls.
- `main_window_comm_page.cpp`: serial channels, network endpoints, and command panels.
- `main_window_log_settings_pages.cpp`: existing analysis page, existing settings page, and log page.

The declarations stay in `include/dss/ui/main_window.h`. Existing private helper methods keep their names so tests and call sites do not change. The split is mechanical: no behavior changes, no new widgets, and no altered signal semantics.

## Storage Architecture

`LocalImageStorageBackend` and `TrackDataStorageBackend` already use background workers and queues. This slice makes those queues production safer by adding:

- A configurable maximum pending queue size with conservative defaults.
- A dropped request counter exposed through a const accessor.
- A failure return from enqueue when the worker is stopped or the queue is full.

The storage workers still drain accepted requests on stop. When the queue is full, the new request is rejected instead of allowing unbounded memory growth.

## Logging Architecture

The existing `Logger` keeps publishing `LogMessageEvent` for UI consumption. This slice adds optional file output:

- File logging is explicitly enabled by path.
- Parent directories are created when possible.
- File sink failures are reported as `std::expected` errors.
- Event-bus logging remains active whether file logging is enabled or not.

The implementation should avoid making tests depend on global process logging state. Tests should configure, write, flush/shutdown where needed, and isolate output in temporary directories.

## Error Handling

UI split errors should be compile-time only; behavior must remain unchanged.

Storage enqueue methods return `false` when work cannot be accepted. Dropped counters let ViewModels or future diagnostics surface backpressure without changing the current UI in this slice.

Logger file setup returns `std::expected<void, std::string>` so callers can decide whether a file logging failure is fatal. Existing event-bus logging is not disabled by file setup failure.

## Testing

The work will use TDD for behavioral changes:

- Add storage tests that fill each backend queue to capacity and verify the next enqueue is rejected and counted.
- Add logger tests that enable file logging, emit messages, and verify the file contains the expected records.
- Keep existing UI tests as regression coverage for the mechanical `MainWindow` split.

Verification target: `ctest --test-dir build\\msvc-debug --output-on-failure` remains green.

## Risks

The UI split touches a large file, so the main risk is accidentally changing signal wiring or widget ownership. Keeping function names and declarations stable limits this risk.

Storage backpressure changes can affect callers that assume enqueue always succeeds. Current tests already cover stopped-worker rejection, and new tests will document full-queue rejection.

Logger changes touch process-global logging. Tests must isolate file paths and avoid requiring a specific global sink order.
