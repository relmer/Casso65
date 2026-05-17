# Feature Specification: Disk II Debug Window

**Feature Branch**: `feature/006-disk-ii-debug`
**Created**: 2026-05-17
**Status**: Draft
**Input**: User description: "Add a debug window to the Casso //e emulator that surfaces a live, scrolling, append-only log of Disk II controller events (motor on/off, head step, head bump at track-0 or upper stop, address-mark sighting, data-mark read, data-mark write, drive select, disk insert/eject). The window opens from View → Disk II Debug..., is modeless, lives alongside the main emulator window, and uses a Win32 virtual-mode ListView for the log. The event source is a new IDiskIIEventSink interface fired by DiskIIController; a passive nibble-stream watcher inside the controller decodes 4-and-4 address marks and detects 6-and-2 data-mark prologues so address-mark and data-mark events can be emitted without disturbing the existing nibble engine. Cross-thread delivery uses a lock-free single-producer single-consumer ring buffer drained on a UI timer. Auto-tail behavior keeps the view pinned to newest events unless the user has scrolled back into history. Filter checkboxes (Motor, Head, Sector R/W, Door, DriveSelect), Pause/Resume, and Clear buttons. Single controller in v1."

**Reference research**: [`research.md`](./research.md) — Disk II nibble-format anchors (D5 AA 96 / D5 AA AD), Win32 virtual-mode ListView idioms, SPSC ring patterns, and the Casso-specific touch points fingerprinted in `DiskIIController.{h,cpp}` and the shell.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Watch a DOS 3.3 cold boot fire address marks in real time (Priority: P1)

A developer is investigating why a particular WOZ image hangs at boot. They open **View → Disk II Debug...** before mounting the image, then mount the disk and cold-boot. As the controller spins the drive and RWTS walks the head from rest to track 0, the debug window shows a live stream: `MOTOR ON`, a flurry of `HEAD STEP` rows (each with a `qt N -> qt M (inward|outward)` detail), several `HEAD BUMP` rows at the track-0 stop, and then — as soon as the head settles and the nibble engine starts feeding the controller — a steady cadence of `ADDR MARK T0 S0 V254`, `ADDR MARK T0 S1 V254`, … followed by `DATA READ S0 (256 bytes)` rows. The view stays pinned to the bottom row as new events stream in; the user can read the tail without touching the scroll bar.

**Why this priority**: This is the entire feature's reason to exist. If a developer can't see a DOS 3.3 boot's address-mark stream, the feature has failed. It exercises every event type the v1 sink defines in a single 5-second sequence on real software, and it validates both the auto-tail scroll heuristic and the cross-thread queue under peak DOS 3.3 event load (~150 events/sec).

**Independent Test**: Open the debug window before mounting a DOS 3.3 fixture disk in the //e profile, cold-boot, and visually verify the four-event-class burst pattern (motor → head step+bump → address mark → data read) appears in order and stays at the tail of the list. Verify no rows are dropped (no `[N events lost]` marker) during the boot.

**Acceptance Scenarios**:

1. **Given** the debug window is open and a DOS 3.3 disk is mounted, **When** the user cold-boots, **Then** within ~50 ms of the first `$C0E9` strobe a `MOTOR ON` row appears at the bottom of the list and remains visible without manual scrolling.
2. **Given** the boot proceeds through RWTS recalibration, **When** the head pins against the track-0 stop, **Then** at least one `HEAD BUMP` row appears with detail `at qt 0`, and consecutive bump events produce one row each (no collapsing).
3. **Given** the nibble engine begins feeding sector data, **When** the controller observes a complete `D5 AA 96` address-mark prologue + 4-and-4 (volume, track, sector, checksum) + `DE AA EB` epilogue, **Then** an `ADDR MARK` row appears with detail in the form `T<track> S<sector> V<volume>` (e.g., `T0 S0 V254`).
4. **Given** the address mark was successfully observed, **When** the controller subsequently observes the `D5 AA AD` data-mark prologue and reads 342 6-and-2 nibbles before the `DE AA EB` epilogue, **Then** a `DATA READ` row appears with detail `S<sector> (256 bytes)`.
5. **Given** the user has not touched the scroll bar, **When** the list grows by hundreds of rows during the boot, **Then** the bottom-most row stays visible after each batch of inserts (auto-tail behavior).

---

### User Story 2 - Scroll back to inspect history without losing it (Priority: P2)

A developer noticed an anomaly mid-boot and wants to scroll back to read it. They drag the ListView's scroll thumb upward to a row of interest; new events continue to arrive on the CPU thread but the view stays put — the user reads the row, copies it to the clipboard with Ctrl+C, and then jumps back to the tail by pressing End (or scrolling to bottom) to resume tail-following.

**Why this priority**: Without this, the feature is a strobe light — useful for spotting bursts, useless for inspecting them. The auto-tail heuristic is what makes the window readable under real load.

**Independent Test**: With the debug window open and a disk actively being read, scroll up by ~50 rows. Verify (a) the scroll position does not jump back to the bottom as new rows are appended, (b) the row count in the status area (or the ListView footer) increases as new events arrive, (c) pressing Ctrl+End or scrolling to the bottom re-enables tail-following on the next append.

**Acceptance Scenarios**:

1. **Given** the debug window is open with disk activity in progress, **When** the user scrolls up such that the last visible row is no longer the actual last row, **Then** subsequent event appends MUST NOT scroll the view; the user's current scroll offset MUST be preserved.
2. **Given** the user is reading mid-history, **When** they scroll back to the bottom so the last visible row equals the last row in the list, **Then** the next event append MUST scroll the view to keep the new bottom row visible.
3. **Given** the user has selected one or more rows, **When** they press Ctrl+C, **Then** the selected rows MUST be copied to the clipboard as tab-separated text in the column order (time, cycle, event type, detail).

---

### User Story 3 - Filter, pause, and clear without losing in-flight events (Priority: P2)

A developer is hunting a head-step regression and wants to see only `HEAD STEP` and `HEAD BUMP` rows. They uncheck the **Motor**, **Sector R/W**, **Door**, and **DriveSelect** filter boxes. The view collapses to head events only. They re-check **Sector R/W** mid-session; the address-mark and data-mark rows that flowed in while the filter was off now appear in the view (filtering is a projection, not a drop). They press **Pause** to freeze the view and read at leisure; events continue to accumulate in the ring buffer. When the ring fills, a single synthetic `[N events lost]` row is inserted on the next successful drain. They press **Resume**, observe a few new rows, then press **Clear** — the displayed deque empties and the row count returns to zero, but events still in the ring continue to drain into the now-empty deque.

**Why this priority**: Filtering, pausing, and clearing are the three controls that turn a firehose into a debugger. Each is independently testable and each addresses a distinct workflow problem (signal-to-noise, "let me read this", "let me start fresh from here").

**Independent Test**: With disk activity in progress, (a) uncheck all filters except Head; verify only `HEAD STEP` / `HEAD BUMP` rows are visible; re-check Sector R/W and verify the historical sector rows reappear. (b) Press Pause; verify the row count freezes; wait long enough that the ring would overflow (or force overflow with a test hook); resume; verify a `[N events lost]` marker appears with N matching the dropped count. (c) Press Clear; verify the displayed list goes to 0 rows; verify subsequent disk activity produces fresh rows starting from 1.

**Acceptance Scenarios**:

1. **Given** the Motor filter checkbox is unchecked, **When** the controller fires `OnMotorStart` / `OnMotorStop`, **Then** the corresponding rows MUST NOT be displayed; re-checking Motor MUST cause those previously-hidden rows to appear in chronological order alongside other rows.
2. **Given** Pause is pressed, **When** the CPU thread continues to push events into the ring, **Then** the displayed row count MUST NOT change until Resume is pressed.
3. **Given** the ring buffer fills while paused (or while the UI is too slow), **When** the next successful drain occurs, **Then** exactly one synthetic `[N events lost]` row MUST be inserted into the deque at the position where the loss occurred (not at the tail of the deque), with N equal to the count of dropped events.
4. **Given** Clear is pressed, **When** the displayed row count is reset to zero, **Then** the underlying ring buffer MUST NOT be flushed; in-flight events MUST continue to drain into the (now-empty) deque on subsequent timer ticks.

---

### User Story 4 - Custom-protected loader investigation (Priority: P3)

A developer is investigating why a specific copy-protected disk (e.g., Karateka) loads on real hardware but fails on Casso. They open the debug window and boot the protected disk. Instead of the clean `T0 S0 V254`, `T0 S1 V254`, … cadence of a stock DOS 3.3 disk, they see a stream of `ADDR MARK` rows with non-standard volume numbers, unusual track-step patterns (`HEAD STEP qt 17 -> qt 18 (inward)` for half-track reads), and `DATA READ` rows for sectors that the standard 16-sector format doesn't contain. The pattern itself becomes the diagnostic: comparing the live stream against a known-good capture on real hardware (or against another emulator's log) reveals where the controller's behavior diverges.

**Why this priority**: Tracks the spirit of #67 / #68 / #69. The feature doesn't *solve* copy-protection bugs but it makes them *visible*, which is the prerequisite for fixing them.

**Independent Test**: Mount a known copy-protected WOZ image with the debug window open, attempt to boot, and verify the event stream surfaces non-standard volume numbers, half-track steps, or unusual sector reads consistent with the protection scheme on file for that title.

**Acceptance Scenarios**:

1. **Given** a copy-protected disk is being read, **When** the address-mark watcher decodes an address field with a non-default volume number (any V ≠ 254), **Then** the `ADDR MARK` row's detail MUST display the actual decoded volume number.
2. **Given** a copy-protected loader steps the head to a half-track or quarter-track position, **When** the controller's `HandlePhase()` invocation produces a `qtDelta` of magnitude 1, 2, or 3 (not a full 4), **Then** the corresponding `HEAD STEP` row MUST show the actual `prev qt` and `new qt` values so the half-track motion is visible.
3. **Given** the loader reads a sector whose address-mark didn't appear in the immediately preceding ten rows, **When** the `DATA READ` row is emitted, **Then** the row MUST still appear (the watcher MUST NOT require a paired address mark to emit a data-mark event).

---

### Edge Cases

- **Ring overflow under sustained CPU-thread firehose**: A user opens the debug window, presses Pause, and lets the //e run a tight loop that hammers `$C0E0`–`$C0E7` (head-step soft switches). The ring fills within ~30 ms. On Resume, exactly one `[N events lost]` row MUST appear with N equal to the dropped count, regardless of how many overflows occurred during the pause.
- **Window opened mid-session**: A user opens **View → Disk II Debug...** after a disk has been spinning for ten seconds. The list MUST start empty — no historical replay. The window only shows events that fire from the moment it is opened (or, more precisely, from the moment the event sink is attached, which happens on window construction).
- **Window closed while events are firing**: Closing the debug window MUST detach the sink, stop the UI timer, and free the deque. The CPU thread MUST NOT continue pushing into a torn-down ring; the controller's sink pointer MUST be reset to null atomically before the ring is destroyed.
- **Address mark with bad checksum**: The 4-and-4 address field's checksum byte (volume XOR track XOR sector) MUST be verified by the watcher. A mismatched checksum MUST NOT fire an `ADDR MARK` event — the watcher silently resets to looking for the next `D5 AA 96` prologue. Rationale: garbage bytes routinely look like address-mark prologues; firing on bad checksums would flood the log with false positives.
- **Data mark without a preceding address mark**: As above, a `D5 AA AD` prologue MUST emit a `DATA READ` event regardless of whether an `ADDR MARK` was observed in the recent past. The watcher's two prologue searches are independent state machines. The `DATA READ` row's "sector" field uses the most recently observed address-mark's sector number (cached on the watcher); if no address mark has ever been observed, the detail field shows `S?` for sector.
- **Multiple controllers configured**: Casso's machine config supports two Disk II controllers in principle. v1 of this feature targets controller #0 (slot 6, the conventional Disk II slot). If a future machine config has a Disk II in another slot (typically slot 5), the debug window MUST NOT crash and MUST surface a clear "monitoring controller #0 only" indication in the title bar or status area.
- **Address-mark watcher must not perturb nibble engine**: The watcher is a passive observer of the nibble stream — it MUST NOT consume, modify, reorder, or buffer nibbles in any way that the existing read/write path can observe. The watcher's state and the engine's state MUST remain fully independent (verified by a controller-with-no-sink-attached regression test).
- **Save/restore mid-stream**: Out of scope for this feature — Casso has no save-state subsystem to break here.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001 (Modeless debug window)**: The emulator MUST expose a modeless debug window opened via **View → Disk II Debug...** (menu item id `IDM_VIEW_DISKII_DEBUG`). The window MUST be non-blocking — it lives alongside the main emulator window and MUST NOT halt CPU emulation while open. It MUST follow the same lifecycle pattern as `OptionsDialog` and `MachinePickerDialog`. Closing the window MUST detach the event sink before the underlying queue is destroyed.
- **FR-002 (Accelerator)**: The menu item MUST have a keyboard accelerator of **Ctrl+Shift+D**. The accelerator MUST be registered in the application accelerator table and MUST function whether the main window or any modeless dialog has focus, as long as no modal dialog is in front of it.
- **FR-003 (ListView, virtual + report mode)**: The window MUST contain a Win32 ListView control in `LVS_REPORT | LVS_OWNERDATA` mode. The ListView MUST source its row contents via `LVN_GETDISPINFO` against the UI-thread deque (FR-009); it MUST NOT use `InsertItem` per row.
- **FR-004 (Column set)**: The ListView MUST have exactly four columns in this order:
  1. **Time** — `HH:MM:SS.mmm`, relative to emulator startup (see Assumption A-001).
  2. **Cycle** — cumulative CPU cycle counter since the most recent reset, displayed as a decimal integer with thousands separators (e.g., `1,234,567`).
  3. **Event** — one of: `MOTOR ON`, `MOTOR OFF`, `HEAD STEP`, `HEAD BUMP`, `ADDR MARK`, `DATA READ`, `DATA WRITE`, `DRIVE SELECT`, `DISK INSERTED`, `DISK EJECTED`, `[N events lost]` (synthetic).
  4. **Detail** — free-form per-event text, formatted per FR-005.
- **FR-005 (Per-event detail formatting)**: The detail column MUST be populated as follows:
  | Event | Detail format |
  |---|---|
  | `MOTOR ON` | empty |
  | `MOTOR OFF` | empty |
  | `HEAD STEP` | `qt <prev> -> qt <new> (<direction>)` where direction is `inward` (new > prev) or `outward` (new < prev). |
  | `HEAD BUMP` | `at qt <position>` where position is `0` or `kMaxQuarterTrack` (typically 139). |
  | `ADDR MARK` | `T<track> S<sector> V<volume>` (e.g., `T17 S5 V254`). |
  | `DATA READ` | `S<sector> (<N> bytes)`; if no address mark has been observed, `S?` is used for sector. |
  | `DATA WRITE` | `S<sector> (<N> bytes)`; same fallback as DATA READ. |
  | `DRIVE SELECT` | `drive <N>` (1-based). |
  | `DISK INSERTED` | `drive <N>` (1-based). |
  | `DISK EJECTED` | `drive <N>` (1-based). |
  | `[N events lost]` | `<N>` (decimal). |
- **FR-006 (Event sink interface)**: A new abstract interface `IDiskIIEventSink` MUST be declared in `CassoEmuCore/Devices/IDiskIIEventSink.h`. The interface MUST contain exactly these virtual methods (all `void`, all infallible):
  - `OnMotorStart()`
  - `OnMotorStop()`
  - `OnHeadStep(int prevQt, int newQt)`
  - `OnHeadBump(int atQt)`
  - `OnAddressMark(int track, int sector, int volume)`
  - `OnDataMarkRead(int sector, int bytesRead)`
  - `OnDataMarkWrite(int sector, int bytesWritten)`
  - `OnDriveSelect(int drive)`
  - `OnDiskInserted(int drive)`
  - `OnDiskEjected(int drive)`
- **FR-007 (Controller fires into sink — guarded)**: `DiskIIController` MUST gain a single `IDiskIIEventSink* m_eventSink` pointer (default `nullptr`) and a `SetEventSink(IDiskIIEventSink*)` method. Every fire site MUST be guarded by `if (m_eventSink != nullptr) { ... }`. When `m_eventSink` is `nullptr`, the controller's behavior, performance, and observable state MUST be 100% identical to its pre-feature behavior.
- **FR-008 (Address-mark / data-mark watcher)**: A passive nibble-stream watcher MUST be added inside the controller's nibble read path. The watcher maintains a small state machine that:
  1. Searches for the `D5 AA 96` address-mark prologue.
  2. On match, decodes the next 8 nibbles as 4-and-4 (volume, track, sector, checksum), verifies `checksum == volume XOR track XOR sector`, and on success fires `OnAddressMark(track, sector, volume)`. On checksum mismatch, the watcher silently resumes the prologue search.
  3. Independently, searches for the `D5 AA AD` data-mark prologue. On match, counts subsequent 6-and-2 nibbles until the `DE AA EB` epilogue is observed, then fires `OnDataMarkRead(cachedSector, decodedByteCount)` using the sector number from the most recent address-mark sighting (`S?` fallback handled at the UI layer per FR-005).
  4. The two state machines are independent — neither requires the other to have matched.
  5. The watcher MUST be read-only with respect to the nibble engine: it observes each nibble as it passes through the existing read path and MUST NOT consume, modify, reorder, buffer, or delay nibbles. Existing read-path tests MUST pass byte-identically before and after the watcher is added (verified with the sink attached but with all event-fire sites stubbed to no-op).
- **FR-009 (Cross-thread SPSC queue)**: A fixed-capacity single-producer single-consumer (SPSC) lock-free ring buffer of capacity 4,096 events MUST mediate CPU-thread fire sites (`IDiskIIEventSink` implementation) and the UI-thread debug-window consumer. The CPU thread MUST be the sole producer; the UI thread MUST be the sole consumer. The push operation on the producer side MUST be wait-free and lock-free (single relaxed store + one release-ordered tail update, or equivalent). The CPU thread MUST NEVER block on UI state.
- **FR-010 (Drop-oldest with lossage marker)**: When the ring is full and the producer pushes a new event, the producer MUST drop the new event (or atomically drop the oldest — implementer's choice, but the user-visible behavior MUST be "the most recent burst is preserved if at all possible"). On the next successful drain, exactly one synthetic `[N events lost]` entry MUST be inserted into the UI-thread deque at the position where the loss occurred (interleaved with surviving events in approximate chronological order), with N equal to the count of events dropped since the previous drain. Multiple successive overflows between drains MUST coalesce into a single marker per drain.
- **FR-011 (UI drain timer)**: The UI thread MUST drain the ring on an approximately 30 Hz timer (every ~33 ms). Drains MUST be batched — one timer tick MUST drain all currently-available events from the ring into the deque, then update the ListView's virtual row count via `ListView_SetItemCountEx`. Drains MUST be skipped (without ring activity) when the window is hidden, minimized, or Pause is active.
- **FR-012 (UI backing deque with rolling cap)**: The debug window MUST maintain a `std::deque<DiskIIEventDisplay>` (or equivalent FIFO) of formatted entries with a rolling cap of 100,000 entries. When a drain would push the deque size above the cap, the oldest entries MUST be `pop_front`-ed until the cap is restored. The ListView's virtual row count MUST always equal the deque size.
- **FR-013 (Auto-tail scroll heuristic)**: Before each drain appends new rows, the consumer MUST capture whether the user is currently viewing the tail of the list. "At tail" MUST be defined as: `ListView_GetTopIndex + ListView_GetCountPerPage >= deque.size()` immediately before the append. If at tail, after `ListView_SetItemCountEx`, the consumer MUST call `ListView_EnsureVisible` on the new last index to scroll the new tail row into view. If not at tail, the consumer MUST NOT modify the scroll position.
- **FR-014 (Filter checkboxes)**: The dialog MUST expose five filter checkboxes above the ListView: **Motor**, **Head**, **Sector R/W**, **Door**, **DriveSelect**. Each checkbox MUST default to checked. The checkbox-to-event mapping is:
  - **Motor** → `MOTOR ON`, `MOTOR OFF`
  - **Head** → `HEAD STEP`, `HEAD BUMP`
  - **Sector R/W** → `ADDR MARK`, `DATA READ`, `DATA WRITE`
  - **Door** → `DISK INSERTED`, `DISK EJECTED`
  - **DriveSelect** → `DRIVE SELECT`

  Filtering MUST be applied as a projection from the deque to the displayed view, not at the ring-drain stage. Unchecking and re-checking a filter MUST cause the previously-hidden rows of that class to become visible in their original chronological position.
- **FR-015 (Pause / Resume button)**: A **Pause** button MUST halt the UI drain timer. Events MUST continue to accumulate in the ring buffer (with drop-with-marker behavior per FR-010 if the ring fills). Pressing **Resume** (the same button toggling label) MUST re-enable the drain on the next timer tick.
- **FR-016 (Clear button)**: A **Clear** button MUST empty the UI-thread deque and update the ListView's virtual row count to zero. Clear MUST NOT flush the ring buffer — any events still in flight MUST continue to drain into the now-empty deque on subsequent ticks.
- **FR-017 (Single controller, v1)**: The debug window MUST attach its sink to **controller #0** in the machine config (typically the slot-6 Disk II). If the machine config contains more than one Disk II controller, the window MUST surface a clear "monitoring controller #0 only" indication (e.g., in the window title or a status label) and MUST NOT crash, partially attach, or attempt multi-controller fan-out. Multi-controller support is explicitly deferred (Out of Scope item 1).
- **FR-018 (Window opened mid-session — no replay)**: Opening the debug window MUST NOT replay historical events. The list MUST start empty. The sink is attached at window construction and detached at destruction; only events fired between those two moments are visible.
- **FR-019 (Clipboard copy)**: With one or more rows selected in the ListView, pressing **Ctrl+C** MUST copy the selected rows to the clipboard as tab-separated UTF-16 text in the column order (time, cycle, event, detail), one line per row terminated by CRLF. This MUST be implemented via the standard ListView `LVN_GETDISPINFO` clipboard handler or a manual `WM_COPY` responder — either is acceptable.
- **FR-020 (Backwards compatibility)**: With no sink attached (i.e., the debug window is never opened in a session), the `DiskIIController` MUST behave identically to the pre-feature controller in every observable way: nibble timing, soft-switch latency, sector-read/write byte-identity, head-step cadence. Verified by the existing controller test suite passing byte-identically.

### Non-Functional Requirements

- **NFR-001 (No CPU-thread slowdown)**: The event-fire path on the CPU thread MUST NOT introduce any user-perceptible slowdown in emulation. Each fire site is a `nullptr` check + (if attached) one SPSC ring push (fixed-cost lock-free operation: two unsynchronized loads, one store, one release update). The address-mark watcher's per-nibble cost MUST be bounded to a single small switch statement and at most one state-machine transition. Profile-measured cost in a sustained DOS 3.3 boot MUST be < 1% of total CPU-thread time.
- **NFR-002 (Test isolation)**: Tests MUST NOT touch the filesystem or system state. Unit tests use a mock `IDiskIIEventSink` and in-memory ring/deque instances. The dialog's UI-projection logic (ring drain → deque projection → filtered view) MUST be testable headlessly — no real `HWND`, no real ListView, no real timer. The Win32 wiring layer is exercised by manual / integration testing only.
- **NFR-003 (Memory footprint)**: Ring buffer at 4,096 entries × ~24 bytes/event = ~96 KB. Deque cap at 100,000 entries × ~80 bytes/formatted entry = ~8 MB worst case. These are accepted (NFR comment, not a hard limit).
- **NFR-004 (Dialog responsiveness)**: The UI drain + ListView count update MUST complete in well under one frame at 30 Hz (< 33 ms) for any drain batch the 4,096-entry ring can hold. The ListView's `LVN_GETDISPINFO` callback MUST be O(1) per visible row (deque random access by index).
- **NFR-005 (Constitution compliance)**: All new code MUST follow `.specify/memory/constitution.md`: `Pch.h` first include, EHM error pattern on fallible functions, variable declarations at top of scope with column alignment + pointer column, function-call spacing (`fn (arg)` non-empty, `fn()` empty), C-style cast spacing (`(float) value`), 5 blank lines between top-level constructs, 3 blank lines after top-of-function variable blocks, American English, PascalCase file names, Hungarian for file-scope statics, `std::numbers` for math constants.

### Key Entities

- **`IDiskIIEventSink`** — Abstract notification interface fired by `DiskIIController` at every observable controller event (motor, head, address mark, data mark, drive select, disk insert/eject). Ten infallible methods, all `void`. The contract decouples the controller from any specific consumer (debug window today, profiler or replay-recorder tomorrow). See FR-006.
- **`DiskIIEvent`** — A small POD struct queued from the CPU thread into the ring buffer. Members: event-type enum, CPU cycle counter snapshot, plus a per-type union/struct for the integer payload (track/sector/volume for address mark, prev/new qt for step, drive number, etc.). Bounded to ~24 bytes for ring-footprint efficiency (NFR-003).
- **`DiskIIEventDisplay`** — The UI-thread deque entry. Holds the pre-formatted time string, pre-formatted cycle string, event-type enum (for filter projection), pre-formatted detail string. Producing this struct is the boundary at which CPU-thread events become UI-thread display rows. Bounded to ~80 bytes including small-string-optimized formatting buffers.
- **`DiskIIEventRing`** — SPSC lock-free ring buffer of 4,096 `DiskIIEvent` entries. Single producer = CPU emulation thread; single consumer = UI thread. `TryPush` is wait-free; `TryPopAll` (drain) is wait-free. Overflow returns a count of dropped events (FR-010).
- **`DiskIIAddressMarkWatcher`** — Passive nibble-stream observer with two independent state machines: address-mark prologue+4-and-4 decoder and data-mark prologue+epilogue counter. Owns the most-recently-seen address-mark sector number (for data-mark fallback per FR-005). Fully read-only with respect to the existing nibble engine (FR-008).
- **`DiskIIDebugDialog`** — The modeless Win32 debug window. Owns the UI timer, the deque, the projection cache, the filter checkbox state, and the pause flag. Hosts the virtual-mode ListView. Lifecycle: construct → `SetEventSink` on controller → drain on timer → close detaches sink before destroying ring.
- **Disk II address mark** — The `D5 AA 96` prologue + 4-and-4 encoded fields (volume, track, sector, checksum) + `DE AA EB` epilogue that identifies each sector on a Disk II disk.
- **Disk II data mark** — The `D5 AA AD` prologue + 342 6-and-2 nibbles + `DE AA EB` epilogue that holds 256 bytes of sector data.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer opening **View → Disk II Debug...** (Ctrl+Shift+D) before a DOS 3.3 cold boot sees the full motor → head step + bump → address mark → data read event cadence in chronological order, with the view auto-tailing the latest row, within 100 ms of each event firing.
- **SC-002**: During a sustained DOS 3.3 boot (~150 events/sec peak), the debug window MUST NOT display a `[N events lost]` marker, AND the CPU-thread fire path MUST add < 1 % to total CPU-thread time (measured by sampling profiler against a no-sink baseline).
- **SC-003**: After scrolling up by ≥ 1 page, the user's scroll position remains stable through ≥ 1,000 subsequent event appends; scrolling back to the bottom re-enables auto-tail on the very next append.
- **SC-004**: Toggling any filter checkbox off and on completes within one timer tick (~33 ms) and produces the correct projection (previously-hidden rows of that class reappear in original chronological position).
- **SC-005**: Pressing Pause freezes the displayed row count within one timer tick; pressing Resume produces visible row count growth within one timer tick. A forced ring-overflow during pause produces exactly one `[N events lost]` row on Resume with N equal to the dropped count.
- **SC-006**: Pressing Clear immediately empties the displayed list (row count → 0) and does not lose any events still in flight in the ring buffer (verified by a test that pushes 100 events, calls Clear after drain-of-50, and observes the remaining 50 appear on the next drain).
- **SC-007**: With the address-mark watcher attached but the event sink set to a no-op stub, the existing `DiskIIControllerTests` suite passes byte-identically (same nibble timings, same sector contents, same head positions) as the pre-feature baseline.
- **SC-008**: Mounting a known copy-protected WOZ image (e.g., one of the Apple2/Demos/*.woz fixtures with a non-standard volume number) produces `ADDR MARK` rows with the actual decoded volume number (not 254) for sectors that use a custom volume.
- **SC-009**: Opening the debug window mid-session (10 seconds after a disk has been mounted and is actively reading) starts the list at zero rows; no historical events are replayed; the first visible row is from the next event fired after the window opened.
- **SC-010**: With the debug window closed and the controller running normally, the absence of an attached sink adds zero measurable cost beyond the per-fire-site `nullptr` check (verified by a microbenchmark of the head-step path with and without the sink-aware controller build).

## Out of Scope *(mandatory)*

The following items were considered and explicitly deferred. They MAY become follow-up features but are not delivered by this spec:

1. **Multiple simultaneous Disk II controllers in the debug view** — v1 monitors controller #0 only. A tab control, controller-picker dropdown, or per-controller multiple debug windows is a follow-up.
2. **Memory dump / disassembly / breakpoints / register watch** — the broader CPU-debugger work tracked in #51 / #59. This feature is a passive event log, not a CPU debugger.
3. **Persisting filter checkbox state, pause flag, and column widths across launches** — v1 may reset all dialog state to defaults every launch.
4. **Exporting the log to a file via a Save... button** — Ctrl+C on ListView selection already gives the user a tab-separated copy (FR-019). A dedicated "Save log to file" action is a v2 nicety.
5. **Per-event color coding, severity highlighting, or row icons** — v2 polish.
6. **Decoding 6-and-2 data-mark nibbles into actual byte values** — v1 reports only the count of nibbles observed between data-mark prologue and epilogue. Full nibble-to-byte decoding for display is a separate feature.
7. **Per-track / per-sector statistics panel** — out of scope; v1 is a raw event log only.
8. **Recording the event stream to a file for offline replay** — out of scope; v1 is live-only.
9. **Filter combinations beyond the five checkbox classes** — no per-event-type filtering, no "show only sector N", no regex filters. Out of scope for v1.
10. **Save-state of debug-window state** — Casso has no save-state subsystem; not applicable.

## Assumptions

- **A-001 (Time-of-day clock origin)**: The Time column shows wall-clock time **relative to emulator startup** (i.e., `00:00:00.000` is the moment `EmulatorShell` finished initialization, and times increase from there). Rationale: a 24-hour wall clock places the developer mentally outside the emulator's timeline; a relative clock makes "this event happened 4.512 seconds into the boot" trivially visible. [NEEDS CLARIFICATION: confirm relative-to-startup vs. relative-to-most-recent-reset vs. wall-clock-HH:MM:SS — the spec assumes relative-to-startup until told otherwise.]
- **A-002 (CPU cycle counter)**: The cycle counter displayed in the Cycle column is the cumulative 6502 cycle count since the most recent reset (i.e., the value already maintained by the CPU emulator and currently consumed by `DiskIIController::Tick()` for spindown timing). Resets clear the counter; soft resets do not (matching real hardware).
- **A-003 (Existing DiskIIController invariants)**: `HandlePhase()` correctly computes `qtDelta` and clamps `m_quarterTrack` to `[0, kMaxQuarterTrack]`, and `Tick()` correctly handles the post-soft-switch spindown. The new `OnHeadStep` / `OnHeadBump` fire sites and the address-mark watcher rely on these invariants, which Feature 005 already validated and shipped.
- **A-004 (Single CPU emulation thread is the sole producer)**: All event-sink fire sites are reached only from `ExecuteCpuSlices()` on the CPU emulation thread. The UI thread never directly invokes controller methods. This is the basis for the SPSC ring's correctness (no multi-producer synchronization required).
- **A-005 (Win32 ListView virtual mode is available)**: The shell runs against Common Controls v6 (already required by the existing dialogs); `LVS_OWNERDATA` and `LVN_GETDISPINFO` are available on every supported Windows target (10/11, x64 and ARM64).
- **A-006 (Menu integration)**: `Casso/MenuSystem.{h,cpp}` accepts a new View-menu item and accelerator-table entry without architectural change. The same pattern Feature 005 used for `IDM_VIEW_OPTIONS` applies.
- **A-007 (No machine-config schema change needed)**: The set of `DiskIIController` instances enumerated by the active `MachineConfig` is already accessible to `EmulatorShell` (same access path used by Feature 005's drive-audio source registration). No machine-config XML/JSON schema change is needed for v1.
- **A-008 (Dialog can be designed in code, no .rc dialog template)**: Mirroring `OptionsDialog`'s approach (programmatic control creation in `WM_INITDIALOG`), the debug window's controls (ListView, filter checkboxes, Pause and Clear buttons) may be created in code without a dedicated `.rc` dialog template. The menu and accelerator additions are the only `Casso.rc` touch points.

## Glossary

- **qt / quarter-track** — Disk II head positioning unit. A full physical track is 4 quarter-tracks; the full 35-track surface is 140 quarter-track positions (0..139). The head can be parked at half-track or quarter-track positions; copy-protection schemes exploit this.
- **Address mark** — The `D5 AA 96` prologue + 4-and-4 encoded fields (volume, track, sector, checksum where `checksum = volume XOR track XOR sector`) + `DE AA EB` epilogue that identifies each sector on a Disk II disk. Every sector on a stock DOS 3.3 disk has one.
- **Data mark** — The `D5 AA AD` prologue + 342 6-and-2 encoded nibbles + `DE AA EB` epilogue that holds 256 bytes of sector data. Follows the address mark for its sector.
- **4-and-4 encoding** — A nibble encoding that splits each byte into two 7-bit nibbles (odd and even halves). Used in address marks because every nibble's top bit can be forced high (Disk II hardware requirement for "valid" nibbles in the field).
- **6-and-2 encoding** — The denser nibble encoding used inside data marks: every 3 source bytes become 4 disk nibbles. Used in DOS 3.3 / ProDOS 16-sector format; the older 13-sector DOS 3.2 format used 5-and-3.
- **Volume number** — The V field in the address mark (range 0–254 in DOS 3.3, default 254 on `INIT`). Most stock software does not enforce a match; copy-protected loaders sometimes do.
- **RWTS** — Read/Write Track and Sector, DOS 3.3's disk I/O layer. The DOS code that actually drives `DiskIIController` to read sectors.
- **Auto-tail** — The scroll behavior that pins the visible window to the bottom of an append-only list as long as the user is reading the tail, but freezes the scroll position once the user scrolls back into history.
- **SPSC ring** — Single-producer single-consumer lock-free ring buffer. Requires only that producer and consumer indices be updated with release/acquire ordering on a single thread each; needs no compare-exchange and no kernel-level synchronization primitive.
- **Modeless dialog** — A Win32 dialog that does not block its owner's message loop. The user can interact with the main window and the dialog at the same time. Contrast with modal dialogs (file open, settings dialogs that take focus).
