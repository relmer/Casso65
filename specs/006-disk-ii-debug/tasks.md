---
description: "Disk II Debug Window — actionable, dependency-ordered tasks"
---

# Tasks: Disk II Debug Window

**Input**: Design documents from `/specs/006-disk-ii-debug/`
**Prerequisites**: spec.md, research.md, plan.md
**Constitution**: `.specify/memory/constitution.md`

**Tests**: Required throughout. Every production-code task that has
observable behavior has a paired test task. Tests use a mock
`IDiskIIEventSink`, in-memory `DiskIIEventRing` instances, and
in-memory nibble streams — no host filesystem reads, no audio device,
no real `HWND`, no real timer, per constitution §II and NFR-002.

**Organization**: Phases follow `plan.md` §"Phasing" — interface first,
producer side, consumer side, Win32 wiring, polish. Each phase ends with
an explicit `[GATE]` task that re-asserts pre-existing tests still green
and that the no-sink controller path is byte-identical to the pre-feature
baseline (FR-007, FR-020, SC-007).

## Format: `[ID] [P?] [GATE?] Description`

- **[P]** — parallel-safe within its phase (different files, no shared-edit conflict)
- **[GATE]** — phase-completion gate task; not parallel
- Every task cites the FR(s) it satisfies and the file(s) it touches
- Every production-code task is paired with (or precedes) a test task

## Constitution rules — apply to every code task

1. Every `.cpp` includes `"Pch.h"` as the **first** include; no angle-bracket
   includes anywhere except inside `Pch.h`; project headers use quoted includes
2. Function comments live in `.cpp` (80-`/` delimiters), not in the header
3. Function/operator spacing: `func()`, `func (a, b)`, `if (...)`, `for (...)`
4. All locals at top of scope, column-aligned (type / `*`-`&` column / name / `=` / value)
5. No non-trivial calls inside macro arguments — store result first
6. ≤ 50 lines per function (100 absolute); ≤ 2 indent levels beyond EHM (3 max)
7. Exactly 5 blank lines between top-level constructs; exactly 3 between
   top-of-function declarations and first statement
8. EHM pattern (`HRESULT hr = S_OK; … Error: … return hr;`) on every fallible
   function; never bare `goto Error`
9. No magic numbers (except 0/1/-1/nullptr/sizeof) — see `plan.md` Constitution
   table for the named-constants list
10. File names: PascalCase, no underscores (`DiskIIEventRing.cpp`, not
    `disk_ii_event_ring.cpp`)
11. C-style cast spacing: `(float) value` with one space before the value
12. Hungarian for file-scope statics (`g_pszWindowClass`, `s_kColumnWidths`)
13. American English throughout

---

## Phase 0: Contracts & Declarations

**Purpose**: Stand up interfaces and types. No behavior yet.

- [ ] T001 [P] Create `CassoEmuCore/Devices/IDiskIIEventSink.h` declaring the
      abstract event-sink interface with exactly the 10 methods listed in
      FR-006 (`OnMotorStart`, `OnMotorStop`, `OnHeadStep`, `OnHeadBump`,
      `OnAddressMark`, `OnDataMarkRead`, `OnDataMarkWrite`, `OnDriveSelect`,
      `OnDiskInserted`, `OnDiskEjected`). All `void`, all infallible.
      Header-only; no STL includes beyond what is in `Pch.h`. (FR-006)
- [ ] T002 [P] Create `CassoEmuCore/Devices/DiskIIEvent.h` declaring the
      `DiskIIEvent` POD struct: event-type enum (`MotorOn`, `MotorOff`,
      `HeadStep`, `HeadBump`, `AddrMark`, `DataRead`, `DataWrite`,
      `DriveSelect`, `DiskInserted`, `DiskEjected`, `EventsLost`), `uint64_t`
      cycle counter snapshot, and a payload union covering the per-type
      integer payload (qt prev/new for step, qt for bump, track/sector/volume
      for addr mark, sector/byte-count for data mark, drive number for
      select/insert/eject, count for lost). Total size ≤ 24 bytes; verify
      with a `static_assert`. (FR-006, NFR-003)
- [ ] T003 [P] Create `Casso/DiskIIEventDisplay.h` declaring the UI-thread
      display struct: pre-formatted time string (small-buffer-optimized,
      e.g., `std::array<wchar_t, 16>`), pre-formatted cycle string
      (e.g., `std::array<wchar_t, 24>`), event-type enum (mirroring
      `DiskIIEvent`'s enum), pre-formatted detail string
      (`std::wstring` or `std::array<wchar_t, 48>`). Total size ≤ 80 bytes
      target; verify with `static_assert` if using fixed buffers. (FR-005,
      NFR-003)
- [ ] T004 [P] Create `CassoEmuCore/Devices/DiskIIEventRing.h` per plan.md
      §"SPSC Ring Buffer" — class declaration only, with `kCapacity = 4096`
      named constant, `TryPush`, `TryPop`, `Drain (DiskIIEvent*, uint32_t)`,
      `ApproxSize`. Two `alignas(64)` `std::atomic<uint32_t>` index members.
      Header-only; no `.cpp` yet. (FR-009, NFR-003)
- [ ] T005 [P] Create `CassoEmuCore/Devices/DiskIIAddressMarkWatcher.h`
      declaring the class with `ObserveNibble (uint8_t nibble)` and
      `SetEventSink (IDiskIIEventSink*)`. Member fields: address-mark and
      data-mark state enums, accumulator fields (vol/trk/sec/chk halves),
      cached most-recent sector number, 3-nibble rolling window for
      epilogue peek-ahead, data-mark body counter. Named constants for the
      prologue/epilogue bytes and `kDataNibbleCount = 342`. (FR-008)
- [ ] T006 Add `CassoEmuCore/Devices/{IDiskIIEventSink,DiskIIEvent,DiskIIEventRing,DiskIIAddressMarkWatcher}.h`
      and `Casso/DiskIIEventDisplay.h` to their respective vcxproj files
      under the header item-group. Build must succeed (declarations only,
      no `.cpp` yet).
- [ ] T007 [GATE] Run `scripts\Build.ps1` and the full unit-test suite;
      verify no regressions from header-only additions.

---

## Phase 1: SPSC Ring Buffer

**Purpose**: Implement and exhaustively test the SPSC ring.

- [ ] T010 Create `CassoEmuCore/Devices/DiskIIEventRing.cpp`. Implement
      `TryPush`, `TryPop`, `Drain`, `ApproxSize` per `research.md` §4.2.
      Use `std::memory_order_relaxed` on same-side index, acquire/release
      on cross-side. (FR-009)
- [ ] T011 [P] Create `UnitTest/Devices/DiskIIEventRingTests.cpp`. Single-
      threaded tests: empty pop returns false; push fills to capacity; one-
      past-capacity push returns false without corrupting state; drain
      empties; wrap-around (push 4096, pop 2048, push 2048, drain 4096)
      preserves FIFO order. (FR-009)
- [ ] T012 [P] In `DiskIIEventRingTests.cpp`, add a two-threaded stress
      test: one producer pushing 100,000 monotonically-increasing payloads,
      one consumer draining; assert no torn reads, no out-of-order pops,
      pushes that returned `false` produce a gap in the consumer's
      observed sequence that matches the producer's count of failed
      pushes. (FR-009, FR-010)
- [ ] T013 [GATE] Run the new ring tests under both Debug and Release
      configurations (Release is required because relaxed-ordering bugs
      are typically masked by Debug's lock-step memory model on x86).

---

## Phase 2: Address-Mark / Data-Mark Watcher

**Purpose**: Implement and test the passive nibble-stream watcher.

- [ ] T020 Create `CassoEmuCore/Devices/DiskIIAddressMarkWatcher.cpp`.
      Implement `ObserveNibble`, `StepAddrMarkState`, `StepDataMarkState`,
      and the 4-and-4 decoder helper. Verify the address-mark checksum
      (`chk == vol XOR trk XOR sec`); on mismatch, silently reset to
      `S_ADDR_IDLE`. Fire `OnAddressMark` only on checksum success. For
      data marks, fire `OnDataMarkRead (cachedSector, 256)` on epilogue
      detection within the expected window. (FR-008)
- [ ] T021 [P] Create `UnitTest/Devices/DiskIIAddressMarkWatcherTests.cpp`.
      Tests:
      - Stock DOS 3.3 address-mark stream (D5 AA 96 + 4-and-4 fields with
        good checksum + DE AA EB) fires exactly one `OnAddressMark` with
        decoded (vol, trk, sec).
      - Same stream with corrupted checksum fires zero `OnAddressMark`.
      - Random-nibble torture stream (1 MB of `rand() & 0xFF`) fires
        zero `OnAddressMark` (false-positive guard).
      - Data-mark stream (D5 AA AD + 342 nibbles + DE AA EB) fires exactly
        one `OnDataMarkRead`.
      - Data-mark without preceding address mark still fires
        `OnDataMarkRead` (sector field falls back to cached `S?` indicator,
        formatted at the UI layer).
      - Interleaved address + data marks (the normal sector cadence) fire
        in expected order with `OnDataMarkRead` using the most recently
        decoded sector number.
      (FR-008)
- [ ] T022 [GATE] Run watcher tests; verify all pass.

---

## Phase 3: Controller Integration

**Purpose**: Wire `IDiskIIEventSink*` + watcher into `DiskIIController`
without changing observable behavior.

- [ ] T030 In `CassoEmuCore/Devices/DiskIIController.h`, add
      `IDiskIIEventSink* m_eventSink = nullptr;`, `DiskIIAddressMarkWatcher
      m_addrMarkWatcher;`, and `void SetEventSink (IDiskIIEventSink* sink)
      noexcept;`. (FR-007)
- [ ] T031 In `CassoEmuCore/Devices/DiskIIController.cpp`, implement
      `SetEventSink` (assign pointer; propagate to `m_addrMarkWatcher`).
      Add guarded fire sites at:
      - `HandleSwitch` case `0x9` for `OnMotorStart` (only on
        false→true transition).
      - `Tick` spindown branch for `OnMotorStop` (only on true→false
        transition).
      - `HandlePhase` after `m_quarterTrack += qtDelta`, branch on
        clamp-vs-no-clamp to fire `OnHeadStep (prevQt, newQt)` or
        `OnHeadBump (atQt)` (mutually exclusive).
      - `HandleSwitch` cases `0xA` / `0xB` for `OnDriveSelect (newDrive)`.
      - `MountImage` for `OnDiskInserted (drive)`.
      - `EjectImage` for `OnDiskEjected (drive)`.
      Every fire site guarded by `if (m_eventSink != nullptr) { ... }`.
      (FR-007, FR-006)
- [ ] T032 In `DiskIIController::NibbleReadByte`, call
      `m_addrMarkWatcher.ObserveNibble (returnedNibble)` after the nibble
      is computed but before returning. The watcher's invocation must
      not modify the returned value. (FR-008)
- [ ] T033 [P] Create `UnitTest/Devices/DiskIIControllerEventTests.cpp`
      with a recording mock sink that captures every fired event into a
      `std::vector<DiskIIEvent>`. Tests:
      - Cold-boot fixture sequence (DOS 3.3 in-memory image, ~5000
        CPU-cycle slice) fires events in the expected order: drive select,
        motor on, head steps, head bump(s), address marks, data reads.
      - `HandlePhase` with `qtDelta == 0` fires zero events.
      - `HandlePhase` that would step past `qt 0` fires `OnHeadBump`,
        not `OnHeadStep`, exactly once.
      - `MountImage` / `EjectImage` fire one event each.
      - No-sink build (attach a sink, immediately revoke, then run the
        same fixture) produces byte-identical nibble stream and head
        positions vs. the original baseline `DiskIIControllerTests`.
      (FR-006, FR-007, FR-020, SC-007)
- [ ] T034 [P] Add a regression assertion to
      `DiskIIControllerEventTests.cpp`: with a recording sink attached
      to a stub that records but does not consume from the ring,
      `DiskIIControllerTests`'s existing cold-boot read fixture produces
      the same nibble byte sequence as without the sink. (FR-008, SC-007)
- [ ] T035 [GATE] Run the full unit-test suite. Existing
      `DiskIIControllerTests` MUST pass byte-identically; new
      `DiskIIControllerEventTests` MUST pass.

---

## Phase 4: UI-Thread Projection Helper

**Purpose**: Implement the non-Win32 projection helper that drains the
ring into the deque, formats per FR-005, applies the rolling cap, and
inserts the lossage marker. Pure-data-structure logic.

- [ ] T040 [P] Create `Casso/DebugDialogProjection.h` declaring a
      class with `DrainAndProject (DiskIIEventRing&, std::deque<DiskIIEventDisplay>&,
      uint32_t droppedCount, uint64_t emulatorStartCycle)`, plus a
      `FormatEvent (const DiskIIEvent&, DiskIIEventDisplay&)` helper, plus
      named constants `kDisplayDequeCap = 100000`. (FR-005, FR-010, FR-012)
- [ ] T041 Create `Casso/DebugDialogProjection.cpp`. Implement
      `FormatEvent` per the FR-005 table (time = `HH:MM:SS.mmm` relative
      to emulator startup per A-001; cycle = decimal with thousands
      separators; event = mapped enum-to-fixed-string table; detail per
      event type). Implement `DrainAndProject` per plan.md §"Data Flow
      Per Event" step 5: if `droppedCount > 0`, push a synthetic
      `EventsLost` display entry before draining; drain via
      `ring.Drain`; push each formatted entry; if the deque exceeds
      `kDisplayDequeCap`, `pop_front` until restored. (FR-005, FR-010,
      FR-012)
- [ ] T042 [P] Create `UnitTest/Casso/DebugDialogProjectionTests.cpp`
      (or under `UnitTest/Devices/` if the test project organization
      requires it). Tests:
      - `FormatEvent` produces the exact strings called out by FR-005 for
        every event type (table-driven test).
      - `DrainAndProject` with an empty ring and zero droppedCount leaves
        the deque unchanged.
      - `DrainAndProject` with N events in the ring appends N entries to
        the deque in FIFO order.
      - `DrainAndProject` with droppedCount > 0 inserts exactly one
        `EventsLost` entry at the position where the loss occurred (test
        fixture: pre-populate deque with 2 entries, set droppedCount = 7,
        push 3 events into ring, drain; expect deque order: orig1, orig2,
        [7 events lost], drained1, drained2, drained3).
      - Rolling-cap enforcement: pre-populate deque to `kDisplayDequeCap`,
        drain 1 event, assert deque size still equals the cap and the
        front entry advanced by one.
      (FR-005, FR-010, FR-012)
- [ ] T043 [GATE] Run projection tests; verify all pass.

---

## Phase 5: Dialog Skeleton (Win32 wiring, no event data yet)

**Purpose**: Create the modeless dialog window, ListView in virtual mode,
filter checkboxes, buttons. Render an empty list.

- [ ] T050 Create `Casso/DiskIIDebugDialog.h` declaring the class with
      `Create (HWND parentHwnd)`, `Show()`, `Hide()`, `Destroy()`,
      `AttachSink (DiskIIController&)`, `DetachSink (DiskIIController&)`,
      and the `IDiskIIEventSink` overrides (CPU-thread side: push to ring,
      bump dropped counter on full). Member fields: `HWND m_hwnd`,
      `HWND m_listView`, 5 `HWND m_filterCheckbox[5]`, `HWND m_pauseButton`,
      `HWND m_clearButton`, `DiskIIEventRing m_ring`,
      `std::atomic<uint32_t> m_droppedSinceLastDrain`,
      `std::deque<DiskIIEventDisplay> m_deque`,
      `DebugDialogProjection m_projection`, `uint32_t m_filterMask`,
      `bool m_paused`, `uint64_t m_emulatorStartCycle`,
      `UINT_PTR m_timerId`. (FR-001..FR-004, FR-009, FR-011)
- [ ] T051 Create `Casso/DiskIIDebugDialog.cpp`. Implement `Create`
      (programmatic `CreateWindowEx` for an overlapped, sizeable window
      with the standard frame; size and position from named constants).
      In `WM_CREATE` / `WM_INITDIALOG`-equivalent: create the ListView
      (`LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS`), 4 columns per
      FR-004 (Time, Cycle, Event, Detail) with default widths from named
      constants `kColTimeWidth = 110`, `kColCycleWidth = 110`,
      `kColEventWidth = 110`, `kColDetailWidth = 360`. Create the 5
      filter checkboxes with labels per FR-014, all initially checked.
      Create Pause and Clear buttons. (FR-001, FR-003, FR-004, FR-014)
- [ ] T052 Implement window-class registration (lazy, on first `Create`)
      using a file-scope `static const wchar_t g_pszDebugWndClass[] =
      L"CassoDiskIIDebugWindow";`. Hungarian per constitution. Register
      with `LoadCursor (NULL, IDC_ARROW)` and a default brush. (FR-001)
- [ ] T053 Implement `Show` / `Hide` / `Destroy` and a basic
      `WM_CLOSE` handler that hides (does not destroy). `WM_DESTROY`
      kills the timer and tears down state. (FR-001)
- [ ] T054 [P] Create `UnitTest/Devices/DiskIIDebugDialogTests.cpp`
      covering the non-Win32 portions: dialog can be default-constructed,
      its filter mask defaults to "all bits set" (FR-014), `m_paused`
      defaults to false, `m_emulatorStartCycle` is recorded on first
      drain. (FR-014, FR-015) No `HWND` is created.
- [ ] T055 [GATE] Build the shell, manually open the dialog from a debug
      menu stub, verify the empty window renders with 4 columns, 5
      checkboxes, 2 buttons, and a resizeable frame. (Manual gate;
      automated UI test is out of scope per NFR-002.)

---

## Phase 6: Drain Timer + Auto-Tail + GETDISPINFO

**Purpose**: Wire the 30 Hz drain and the ListView row-fetch path.

- [ ] T060 In `DiskIIDebugDialog.cpp`, implement `WM_TIMER` handler per
      plan.md §"Auto-Tail Scroll Algorithm". Capture `wasAtTail` BEFORE
      invoking projection. Call `m_projection.DrainAndProject(...)`. If
      deque size changed, call `ListView_SetItemCountEx` with
      `LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL`. If `wasAtTail`, call
      `ListView_EnsureVisible` on the new last index. (FR-011, FR-013)
- [ ] T061 Start the timer (`SetTimer (m_hwnd, m_timerId, kDrainTimerMs,
      nullptr)` with `kDrainTimerMs = 33`) in `WM_CREATE` and kill it in
      `WM_DESTROY`. Skip drain (early-return from `WM_TIMER`) when the
      window is hidden or minimized (`IsWindowVisible` returns false) or
      `m_paused` is true. (FR-011, FR-015)
- [ ] T062 Implement `LVN_GETDISPINFO` handler. Translate the `iItem`
      index into a deque index (Phase 7 will introduce the filter-index
      remap; for now, identity mapping). For each requested column,
      return the appropriate string from `m_deque[i]`. (FR-003, FR-005)
- [ ] T063 [P] Add a `DiskIIDebugDialogTests.cpp` headless test for the
      auto-tail decision function: extract the `wasAtTail` computation
      into a free function `bool ComputeWasAtTail (int topIndex, int
      countPerPage, int totalCount)` testable without an HWND. Verify
      truth table: at tail, scrolled-up-by-1, scrolled-up-by-N, empty
      list, partial-last-row visibility. (FR-013)
- [ ] T064 [GATE] Build, manually open the dialog before a DOS 3.3 cold
      boot, observe the event stream tail-following. Verify scrolling
      up freezes the view; verify scrolling back to the bottom re-enables
      auto-tail on the next event.

---

## Phase 7: Filter Projection

**Purpose**: Wire the 5 filter checkboxes to the displayed projection.

- [ ] T070 In `DiskIIDebugDialog.cpp`, handle `WM_COMMAND` for each
      filter checkbox: update `m_filterMask` (bit per category per
      FR-014: bit 0 Motor, bit 1 Head, bit 2 SectorRW, bit 3 Door, bit 4
      DriveSelect). On change, recompute the filter-index vector (or
      invalidate the LV's contents and rely on `LVN_GETDISPINFO` to do
      lazy filtering — implementer's choice per research §6.1). (FR-014)
- [ ] T071 Update the `LVN_GETDISPINFO` handler to honor the filter mask
      via the chosen approach (filter-index vector OR lazy filter). The
      virtual row count reported by `ListView_SetItemCountEx` must equal
      the number of currently-displayed (filtered) rows. (FR-014)
- [ ] T072 [P] Add `DebugDialogProjectionTests.cpp` cases that exercise
      filter projection: with a fixed deque of one event of each type,
      every single-checkbox mask shows only the corresponding events;
      the all-checked mask shows everything; the all-unchecked mask
      shows nothing; re-checking after un-checking shows the events
      again in chronological order. (FR-014)
- [ ] T073 [GATE] Manual test: during a DOS 3.3 boot, toggle each
      filter and verify the displayed events change appropriately and
      restore on re-check.

---

## Phase 8: Pause / Resume / Clear

**Purpose**: Wire the two action buttons.

- [ ] T080 Handle `WM_COMMAND` for the Pause button: toggle `m_paused`,
      update the button label between "Pause" and "Resume". When paused,
      `WM_TIMER` early-returns; events continue to push into the ring;
      ring overflow increments `m_droppedSinceLastDrain` per FR-010.
      (FR-015)
- [ ] T081 Handle `WM_COMMAND` for the Clear button: clear `m_deque`,
      call `ListView_SetItemCountEx (m_listView, 0, ...)`. Do NOT
      flush the ring — `m_ring` and `m_droppedSinceLastDrain` remain
      untouched, so in-flight events drain into the now-empty deque on
      the next timer tick. (FR-016)
- [ ] T082 [P] Add `DebugDialogProjectionTests.cpp` cases:
      - Pause-induced overflow: push 8192 events to a 4096-capacity ring
        with the consumer not draining (simulating pause), then drain
        once; assert `m_droppedSinceLastDrain` was 4096 (or near it) and
        the resulting deque has exactly one `[N events lost]` entry with
        N matching the dropped count.
      - Clear-with-in-flight: drain 50 events, push 100 more (don't
        drain), call Clear (drop deque + count), drain once; assert the
        deque now contains the 100 in-flight events with no marker.
      (FR-010, FR-015, FR-016, SC-005, SC-006)
- [ ] T083 [GATE] Manual: pause during a heavy seek burst, leave paused
      long enough for the ring to overflow, resume, verify exactly one
      `[N events lost]` row appears at the correct position.

---

## Phase 9: Clipboard Copy

**Purpose**: Ctrl+C support per FR-019.

- [ ] T090 Implement a `WM_KEYDOWN` / accelerator handler on the ListView
      for Ctrl+C: enumerate selected items (`ListView_GetNextItem` loop
      with `LVNI_SELECTED`), format each as
      `"<time>\t<cycle>\t<event>\t<detail>\r\n"` (UTF-16, CRLF terminator),
      concatenate into a `std::wstring`, open clipboard, set
      `CF_UNICODETEXT`. (FR-019)
- [ ] T091 [P] Add a headless test that exercises the formatting helper:
      given a vector of `DiskIIEventDisplay` entries (the "selected"
      set), the helper returns the expected tab-separated UTF-16
      string. (FR-019)
- [ ] T092 [GATE] Manual: select 3 rows, Ctrl+C, paste into Notepad,
      verify tab-separated text with CRLF line endings.

---

## Phase 10: Menu / Accelerator / Shell Wiring

**Purpose**: Make the dialog actually reachable from the UI.

- [ ] T100 [P] In `Casso/resource.h`, define `IDM_VIEW_DISKII_DEBUG` with
      the next available menu-item id.
- [ ] T101 In `Casso/Casso.rc`:
      - Under the existing `View` popup menu: add a new menu item
        `MENUITEM "Disk II Debug...\tCtrl+Shift+D", IDM_VIEW_DISKII_DEBUG`.
      - In the `ACCELERATORS` table: add
        `"D", IDM_VIEW_DISKII_DEBUG, VIRTKEY, CONTROL, SHIFT`.
      (FR-001, FR-002)
- [ ] T102 In `Casso/MenuSystem.cpp`, route `WM_COMMAND` for
      `IDM_VIEW_DISKII_DEBUG` to a new `EmulatorShell::OpenDiskIIDebugDialog()`.
- [ ] T103 In `Casso/EmulatorShell.{h,cpp}`, add
      `std::unique_ptr<DiskIIDebugDialog> m_diskIIDebugDialog;` and
      `void OpenDiskIIDebugDialog();`. On first call, lazy-create the
      dialog, attach as sink on the first `DiskIIController` instance in
      the active machine config (controller #0 per FR-017), and `Show`.
      On subsequent calls, `Show` + `SetForegroundWindow`. On dialog
      close: revoke the sink (`SetEventSink (nullptr)`) BEFORE the dialog
      tears down its ring. (FR-001, FR-017, FR-018)
- [ ] T104 If `MachineConfig` reports more than one Disk II controller,
      append " (controller #0 only)" to the dialog title. (FR-017)
- [ ] T105 [GATE] Build, run the shell, verify the menu item appears
      with the keyboard accelerator hint, Ctrl+Shift+D opens the window,
      menu item also opens the window, closing then reopening reuses
      the dialog instance.

---

## Phase 11: Polish

- [ ] T110 Update `CHANGELOG.md` with a "Disk II Debug Window" entry
      under the next-release section (motor/head/address-mark/data-mark
      live event log; Ctrl+Shift+D; modeless; auto-tailing; filterable;
      pausable).
- [ ] T111 Update `README.md` "What's New" / "Debugging" sections to
      mention the debug window and its accelerator. Include one-sentence
      "open the window *before* the operation you want to investigate"
      guidance (FR-018 / Risks table).
- [ ] T112 Manual A/B test the four bundled WOZ fixtures
      (`Apple2/Demos/AppleStellarInvaders.woz`, `Choplifter.woz`,
      `HardHatMack.woz`, `Karateka.woz`) — verify each surfaces a
      reasonable event stream (motor on, head steps, address marks
      with their actual volume numbers, data reads). Note which
      titles surface non-standard volume numbers or half-track reads,
      and document those observations in the PR description as evidence
      for SC-008.
- [ ] T113 Final constitution sweep:
      - `rg -n '#include' CassoEmuCore/Devices/ Casso/` to verify
        every new `.cpp` includes `"Pch.h"` first.
      - `rg -n '\w \(\)' CassoEmuCore/Devices/ Casso/` to verify
        empty-arg call sites use `func()` (no space) and non-empty use
        `func (args)` (with space).
      - Verify variable blocks at function tops are column-aligned per
        the constitution's type/pointer/name/= columns.
      - Verify exactly 5 blank lines between top-level constructs and
        3 blank lines after function-top variable blocks.
- [ ] T114 [GATE] Full build + full test suite + manual smoke test on
      both x64 and ARM64 (constitution requirement).

---

## Dependency notes

- T030 (controller field additions) blocks T031, T032, T033.
- T010 (ring impl) blocks T011, T012, and any later phase that uses the ring.
- T020 (watcher impl) blocks T021 and T032 (which invokes `ObserveNibble`).
- T041 (projection impl) blocks T042 and Phase 6 (T060).
- T050/T051 (dialog scaffold) block T060–T091.
- T100–T103 (shell wiring) is the last gate before manual end-to-end
  testing; do not start before Phases 5–9 are gate-clean.

## Out-of-scope reminders (do NOT add to tasks.md scope)

- Multi-controller debug window (deferred per spec Out of Scope item 1).
- CPU debugger / breakpoints / memory dump (deferred to #51/#59).
- Persisting filter/pause state (Out of Scope item 3).
- Save-log-to-file button beyond Ctrl+C (Out of Scope item 4).
- Per-row color coding (Out of Scope item 5).
- Decoding 6-and-2 data nibbles into byte values (Out of Scope item 6).
