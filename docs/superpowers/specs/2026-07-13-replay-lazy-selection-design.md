# Replay Lazy Selection Design

## Goal

Selecting an image sequence only records the selected paths and updates replay counters. It must
not open, validate, decode, or upload any selected image. Image work begins only after the user
chooses Start Replay, Step Forward, or Step Back.

## State Flow

### Select Sequence

1. Stop an active replay using the existing lifecycle path.
2. Store the selected paths in `ImageSequenceFrameSource`.
3. Set the displayed frame count to the number of selected paths and reset the current frame to
   zero.
4. Keep replay state Ready. Selection does not enter the replay busy state.

Missing, unreadable, or malformed files are accepted at this stage because selection is a
configuration action, not image processing.

### Start Replay

1. Enter replay busy state.
2. On the existing `std::jthread` task, initialize the replay source and select Replay in
   `FrameSourceCoordinator`.
3. Return to the UI thread after initialization succeeds.
4. Start `ImageProcessor` and the active frame source, set grabbing state, and publish the existing
   start event.

Initialization or source-selection errors leave replay stopped, clear busy state, and publish the
error text.

### Step Forward And Back

Each step task first initializes and selects the replay source in the existing background task,
then seeks as needed and loads one frame. Any file error is reported by that action. The UI remains
responsive while replay controls show their existing Loading state.

## Component Boundaries

- `ReplayViewModel` owns UI state, deferred task orchestration, and action-time source selection.
- `ImageSequenceFrameSource` keeps its current initialization and decoding responsibilities; this
  change only moves when those operations are called.
- `FrameSourceCoordinator` keeps its existing source-selection contract.
- No Qt worker thread is introduced; background work continues to use `std::jthread`.

## OpenGL Filtering

RAW texture minification remains `GL_LINEAR`. Magnification uses `GL_NEAREST` so enlarged pixels
remain distinct. The adjacent source comment is written in Chinese and notes that `GL_LINEAR` can
be restored if smooth magnification is preferred.

## Tests

- Selecting valid paths updates counters immediately without entering busy state.
- Selecting a missing or malformed path succeeds without reading it.
- The first Start or Step action reports a deferred file error.
- Existing forward, backward, seek, busy-state, and display tests continue to pass.
