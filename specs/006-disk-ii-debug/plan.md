# Implementation Plan: Disk II Debug Window

**Branch**: `feature/006-disk-ii-debug` | **Date**: 2026-05-17 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/006-disk-ii-debug/spec.md`
**Research basis**: [`research.md`](./research.md) — Disk II nibble-format anchors, Win32 virtual-mode ListView idioms, and SPSC ring patterns
**Constitution**: `.specify/memory/constitution.md`

## Summary

Add a modeless **Disk II Debug** window to the Casso shell that surfaces a
live, scrolling, append-only log of `DiskIIController` events for developers
diagnosing disk behavior — head-stuck-spinning, RWTS-failing-to-find-an-
address-mark, custom-protected-loaders-reading-non-standard-sectors. The
window opens from **View → Disk II Debug...** (Ctrl+Shift+D), hosts a Win32
ListView in virtual + report mode, and tails events at ~30 Hz with an
auto-tail scroll heuristic that pins newest rows to the visible bottom
unless the user has scrolled back into history.

The implementation is structured around three new components in `CassoEmuCore`
(`IDiskIIEventSink`, `DiskIIEventRing`, `DiskIIAddressMarkWatcher`) and one
new dialog in the `Casso` shell (`DiskIIDebugDialog`). The
`DiskIIController` gains a single `IDiskIIEventSink*` pointer (default
`nullptr`) plus fire sites at every existing motor/head/mount call site,
guarded so that a no-sink build is 100% byte-identical to the pre-feature
controller. A lightweight passive nibble-stream watcher decodes 4-and-4
address marks and detects 6-and-2 data-mark prologues without disturbing
the existing read path.

Cross-thread delivery uses a fixed-capacity (4,096-entry) lock-free SPSC
ring buffer; the CPU emulation thread is the sole producer, the UI thread
is the sole consumer (drained on a ~30 Hz timer). Overflow drops events
with a single coalesced `[N events lost]` marker on the next drain. The
UI thread maintains a `std::deque<DiskIIEventDisplay>` of formatted entries
(rolling cap 100,000) that the virtual-mode ListView pulls from via
`LVN_GETDISPINFO`.

This feature is independent of #51 (interactive debugger engine) and #59
(GUI debugger panel) — it is a passive event log, not a CPU debugger.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK only — User32 (modeless dialog +
accelerators), ComCtl32 v6 (virtual-mode ListView). No new third-party
libraries.
**Storage**: None. v1 does not persist filter / pause / column-width state
across launches.
**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New
`DiskIIEventRingTests.cpp`, `DiskIIAddressMarkWatcherTests.cpp`,
`DiskIIControllerEventTests.cpp` (extends the existing controller test
surface), and `DiskIIDebugDialogTests.cpp` (projection-logic-only — no
real `HWND`, no real ListView, no real timer per NFR-002).
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Existing 5-project solution; new code in `CassoEmuCore`
(library: event sink interface, ring, watcher) and `Casso` (GUI shell:
debug dialog). No new project.
**Performance Goals**: Event-fire path on CPU thread MUST add < 1 % to
total CPU-thread time at DOS 3.3 boot peak (~150 events/sec). Per-fire
cost: one `nullptr` check + (if attached) one SPSC ring push (two
unsynchronized loads, one store, one release update). Address-mark watcher
per-nibble cost: one small switch statement + at most one state-machine
transition.
**Constraints**: No regression in `DiskIIController` byte-for-byte behavior
when the sink is unattached (FR-007, FR-020, SC-007). No locks, no
condition variables, no kernel-level sync on the producer or consumer
fast paths (FR-009). Tests do not touch the filesystem or open a real
HWND (NFR-002).
**Scale/Scope**: ~6 new source files (~700–1,000 LOC), ~5 touched files,
~35–55 new test cases. New modeless dialog (small, programmatically
constructed — no .rc dialog template).

## Constitution Check

### Principle I — Code Quality (NON-NEGOTIABLE)

| Rule                                              | How this plan complies                                                                                                                  |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| Pch.h-first include                               | Every new `.cpp` includes `"Pch.h"` as the first include. No angle-bracket includes outside `Pch.h`. STL/Windows headers go via `Pch.h`. |
| Formatting Preservation                           | All edits to existing files are surgical insertions at the line ranges named in `research.md` §3; no existing aligned blocks are touched. |
| EHM on fallible functions                         | Only the dialog's `WM_INITDIALOG` Win32 wiring is fallible (`CreateWindowEx`); it uses the standard `HRESULT hr = S_OK; … Error: … return hr;` pattern. Ring push/pop, watcher transitions, and event-sink fire methods are all infallible (`void`). |
| No calls inside macro arguments                   | Store results first; reviewed per PR.                                                                                                   |
| Single exit via `Error:` label                    | Standard pattern in every new fallible function (dialog setup only).                                                                    |
| Avoid Nesting (≤ 2-3 indent beyond EHM)           | Watcher state machine factored into per-state helper methods (`StepAddrMarkState`, `StepDataMarkState`), each ≤ 2 levels. Drain loop factored into `DrainRing` + `ProjectToView` helpers. |
| Variable Declarations at Top of Scope             | All new functions follow this; column-aligned per project rules (type / `*`-`&` column / name / `=` / value). |
| Function Comments in `.cpp` Only                  | Headers carry only declaration comments; doc blocks live in `.cpp` with 80-`/` delimiters.                                              |
| Function Spacing — `func()` vs `func (a, b)`      | Verified via `rg -n '\w \(\)' CassoEmuCore/Devices/ Casso/` before commit on every PR phase.                                            |
| Smart Pointers                                    | `DiskIIDebugDialog` is owned by `EmulatorShell` directly (composition); no smart pointer needed. `DiskIIEventRing` and `DiskIIAddressMarkWatcher` are direct members of `DiskIIController` / the dialog respectively. |
| PascalCase file names                             | All new source files (`IDiskIIEventSink.h`, `DiskIIEventRing.{h,cpp}`, `DiskIIAddressMarkWatcher.{h,cpp}`, `DiskIIDebugDialog.{h,cpp}`) and test files use PascalCase with no underscores. |
| No magic numbers                                  | `kEventRingCapacity = 4096`, `kDisplayDequeCap = 100000`, `kDrainTimerHz = 30`, `kDrainTimerMs = 33`, `kAddrMarkPrologue0 = 0xD5`, `kAddrMarkPrologue1 = 0xAA`, `kAddrMarkPrologue2 = 0x96`, `kDataMarkPrologue2 = 0xAD`, `kSectorEpilogue0 = 0xDE`, `kSectorEpilogue1 = 0xAA`, `kSectorEpilogue2 = 0xEB`, `kDataNibbleCount = 342` are all named constants. |
| Hungarian for file-scope statics                  | Any file-scope statics in dialog code (e.g., a `static const wchar_t* g_pszWindowClass`) use Hungarian per constitution.                |
| `std::numbers` for math constants                 | No math constants needed by this feature (ring/deque are integer-indexed; time formatting uses integer ms).                            |
| American English                                  | Verified (e.g., "behavior", not "behaviour"; "canceled", not "cancelled").                                                              |

### Principle II — Test Isolation (NON-NEGOTIABLE)

All tests run on the same headless harness as existing `CassoEmuCore`
tests. The ring buffer, watcher, and event-sink mock are pure data
structures with no Win32 / filesystem dependencies. The dialog's
projection logic (ring drain → deque projection → filtered-view computation)
is factored into a non-Win32 helper class (`DebugDialogProjection` or
equivalent) that tests instantiate directly; the Win32 wiring layer
(`WM_INITDIALOG`, `WM_TIMER`, `LVN_GETDISPINFO`, button handlers) is
exercised by manual / integration testing only and is intentionally thin.

### Principle V — Function Size & Structure

The longest behavior-carrying function is `DiskIIAddressMarkWatcher::ObserveNibble`,
which factors as:

```
ObserveNibble (nibble)
  StepAddrMarkState (nibble)                       // ≤ 30 LOC
  StepDataMarkState (nibble)                       // ≤ 30 LOC
```

`DebugDialogProjection::DrainAndProject`:

```
DrainAndProject (ring, deque, filterMask)
  while ring.TryPop (event):                       // ≤ 5 LOC
      FormatEvent (event, displayEntry)            // ≤ 25 LOC
      deque.push_back (displayEntry)
      if deque.size > kDisplayDequeCap:
          deque.pop_front()
  // filter projection is lazy at GETDISPINFO time
```

All helpers stay under ~30 LOC.

### Principle III — Concurrency Discipline

The CPU thread is the sole producer; the UI thread is the sole consumer.
The SPSC ring uses `std::atomic<uint32_t>` head/tail indices with
`memory_order_relaxed` loads on the same-side index and
`memory_order_acquire` / `memory_order_release` on the cross-side index.
No mutex, no condition variable, no critical section. The sink pointer
on `DiskIIController` is a plain `IDiskIIEventSink*` published via
`SetEventSink` only when the dialog is open and revoked (set to `nullptr`)
before the dialog tears down the ring; the publish/revoke is gated by
the dialog's create/destroy lifecycle, which happens on the UI thread
while the CPU thread is paused at slice boundaries (mirrors the existing
sink-attach pattern from Feature 005's drive-audio wiring).

## Architecture

### Components

```
                ┌──────────────────────────────────┐
                │  DiskIIController                │  (CassoEmuCore/Devices)
                │   ─ HandleSwitch                 │──┐
                │   ─ HandlePhase                  │  │ IDiskIIEventSink*
                │   ─ Tick (spindown)              │  │ (default nullptr)
                │   ─ MountImage / EjectImage      │  │
                │   ─ NibbleReadByte ──────────────│──┼─► DiskIIAddressMarkWatcher
                │   ─ NibbleWriteByte              │  │     ─ ObserveNibble (nibble)
                │   ─ SetEventSink                 │  │     ─ fires OnAddressMark,
                └──────────────────────────────────┘  │       OnDataMarkRead via
                                                      │       same m_eventSink
                                                      ▼
                ┌────────────────────────────────────────────────┐
                │  IDiskIIEventSink (abstract, 10 methods)       │  (CassoEmuCore/Devices)
                │   OnMotorStart / OnMotorStop                   │
                │   OnHeadStep (prevQt, newQt)                   │
                │   OnHeadBump (atQt)                            │
                │   OnAddressMark (track, sector, volume)        │
                │   OnDataMarkRead (sector, bytesRead)           │
                │   OnDataMarkWrite (sector, bytesWritten)       │
                │   OnDriveSelect (drive)                        │
                │   OnDiskInserted (drive) / OnDiskEjected (drv) │
                └────────────────────────────────────────────────┘
                                       ▲
                                       │ implements
                                       │
                ┌──────────────────────┴─────────────────────────┐
                │  DiskIIDebugDialog : IDiskIIEventSink          │  (Casso)
                │   ─ owns DiskIIEventRing (4096 entries)        │
                │   ─ owns std::deque<DiskIIEventDisplay> (≤100k)│
                │   ─ filter checkbox mask, pause flag           │
                │   ─ DebugDialogProjection (testable helper)    │
                │                                                │
                │   CPU thread → IDiskIIEventSink methods        │
                │      → ring.TryPush (DiskIIEvent)              │
                │   UI thread → WM_TIMER (~33 ms)                │
                │      → projection.DrainAndProject              │
                │      → ListView_SetItemCountEx                 │
                │      → (if at tail) ListView_EnsureVisible     │
                └────────────────────────────────────────────────┘
                                       ▲
                                       │ owned by
                                       │
                ┌──────────────────────┴──────────────┐
                │  EmulatorShell                      │  (Casso)
                │   ─ owns DiskIIDebugDialog (opt.)   │
                │   ─ View → Disk II Debug... handler │
                │   ─ wires SetEventSink on open      │
                │   ─ revokes sink on close           │
                └─────────────────────────────────────┘
                                       ▲
                                       │ View menu + Ctrl+Shift+D
                                       ▼
                ┌─────────────────────────────────────┐
                │  MenuSystem + Casso.rc/resource.h   │  (Casso)
                │   IDM_VIEW_DISKII_DEBUG             │
                │   View → Disk II Debug...           │
                │   Accelerator: Ctrl+Shift+D         │
                └─────────────────────────────────────┘
```

### Data Flow Per Event

1. **CPU thread**: an emulation slice runs inside `ExecuteCpuSlices()`.
2. `DiskIIController::HandleSwitch` / `HandlePhase` / `Tick` / `MountImage` /
   `EjectImage` reaches a fire site. Each site is:
   ```cpp
   if (m_eventSink != nullptr) {
       m_eventSink->OnHeadStep (prevQt, newQt);
   }
   ```
3. `DiskIIDebugDialog::OnHeadStep` (the concrete sink) packs a `DiskIIEvent`
   POD (event-type enum + cycle counter snapshot + integer payload union)
   and calls `m_ring.TryPush (event)`. On ring full, the push returns
   `false`; the dialog increments a `m_droppedSinceLastDrain` counter
   (atomic, written only on the CPU thread).
4. The nibble-read path (`DiskIIController::NibbleReadByte`) calls
   `m_addrMarkWatcher.ObserveNibble (nibble)` on every nibble that goes
   to the CPU. The watcher's state machines may fire
   `m_eventSink->OnAddressMark` or `m_eventSink->OnDataMarkRead` from
   inside `ObserveNibble`, which routes through the same SPSC ring push.
5. **UI thread**: every ~33 ms, `WM_TIMER` fires. The handler calls
   `m_projection.DrainAndProject (m_ring, m_deque, m_filterMask, m_droppedSinceLastDrain)`,
   which:
   - Pops every available event from the ring into the deque, formatting
     each into a `DiskIIEventDisplay` (time string, cycle string with
     thousands separators, event-type enum, detail string).
   - If `m_droppedSinceLastDrain > 0`, inserts a single synthetic
     `[N events lost]` entry into the deque at the position where the
     loss occurred (between the last surviving pre-overflow event and the
     first survivor after the overflow).
   - If the deque size exceeds `kDisplayDequeCap` (100,000), `pop_front`-s
     entries until the cap is restored.
6. The handler captures the current "at tail" state by computing
   `(ListView_GetTopIndex + ListView_GetCountPerPage) >= prevDequeSize`,
   then calls `ListView_SetItemCountEx (lv, deque.size(), LVSICF_NOSCROLL)`.
   If "at tail" was true, it calls `ListView_EnsureVisible (lv, deque.size() - 1, FALSE)`.
7. The ListView's `LVN_GETDISPINFO` handler responds to row-fetch requests
   by indexing into `m_deque` (O(1) random access) and returning the
   appropriate string for the requested column. Filter projection is
   applied here: if the entry's event type is filtered out, the handler
   returns an empty row (preferred) or the dialog maintains a separate
   `std::vector<uint32_t>` of indices into the deque that match the
   current filter mask (final choice deferred to implementation).

### Event Trigger Specification

Normative for this feature:

| Event                  | Trigger                                                                  | Call site                                                      |
|------------------------|--------------------------------------------------------------------------|----------------------------------------------------------------|
| `OnMotorStart`         | `m_motorOn` transitions false → true                                     | `HandleSwitch` case `0x9`                                      |
| `OnMotorStop`          | `m_motorOn` transitions true → false (post-spindown)                     | `Tick` after spindown counter expires                          |
| `OnHeadStep`           | `qtDelta != 0` AND head NOT clamped against a stop                       | `HandlePhase` after `m_quarterTrack += qtDelta`                |
| `OnHeadBump`           | `qtDelta != 0` AND post-clamp `m_quarterTrack == 0` (from < 0) OR `== kMaxQuarterTrack` (from > kMaxQuarterTrack) | Same `HandlePhase` site |
| `OnAddressMark`        | Watcher decoded `D5 AA 96` + 4-and-4 fields + verified XOR checksum      | `DiskIIAddressMarkWatcher::StepAddrMarkState` accept terminal  |
| `OnDataMarkRead`       | Watcher decoded `D5 AA AD` + counted 342 6-and-2 nibbles + saw `DE AA EB` | `DiskIIAddressMarkWatcher::StepDataMarkState` accept terminal  |
| `OnDataMarkWrite`      | Equivalent watcher accept on the write path                              | (write watcher mirrors read watcher; v1 minimum: fire on the existing controller-side write completion, deferred to Phase 4 if write-path nibble observation is non-trivial) |
| `OnDriveSelect`        | Active drive index changes via `$C0EA` / `$C0EB`                         | `HandleSwitch` cases `0xA` / `0xB`                             |
| `OnDiskInserted`       | Disk image newly mounted (mid-session; cold-boot mounts not suppressed for the debug log — every mount fires, unlike audio FR-013) | `MountImage` (or shell-level mount path)         |
| `OnDiskEjected`        | Disk image unmounted                                                     | `EjectImage` (or shell-level eject path)                       |

`OnHeadStep` and `OnHeadBump` are mutually exclusive for any single
`HandlePhase` invocation (matches Feature 005's audio plumbing).

### Address-Mark / Data-Mark Watcher State Machines

Two independent state machines inside `DiskIIAddressMarkWatcher`. Each
state machine processes one nibble per `ObserveNibble` call (the same
nibble is fed to both).

**Address-mark state machine** (8 states):

```
S_ADDR_IDLE  ─ on D5 → S_ADDR_D5
S_ADDR_D5    ─ on AA → S_ADDR_AA
             ─ else  → S_ADDR_IDLE (or S_ADDR_D5 if nibble is D5 — overlap)
S_ADDR_AA    ─ on 96 → S_ADDR_VOL_HI (capturing fields)
             ─ else  → S_ADDR_IDLE
S_ADDR_VOL_HI / S_ADDR_VOL_LO  ─ accumulate volume nibbles → S_ADDR_TRK_HI
S_ADDR_TRK_HI / S_ADDR_TRK_LO  ─ accumulate track nibbles → S_ADDR_SEC_HI
S_ADDR_SEC_HI / S_ADDR_SEC_LO  ─ accumulate sector nibbles → S_ADDR_CHK_HI
S_ADDR_CHK_HI / S_ADDR_CHK_LO  ─ accumulate checksum nibbles
                               → on accept: verify `chk == vol XOR trk XOR sec`
                                 if ok: fire OnAddressMark, cache sector, → S_ADDR_IDLE
                                 if bad: → S_ADDR_IDLE silently
```

4-and-4 decode: each pair (`hi`, `lo`) decodes to `((hi << 1) | 1) & lo`,
i.e., the standard `((odd << 1) | 1) & even` formula used by every
Apple II disk decoder.

**Data-mark state machine** (4 states + counter):

```
S_DATA_IDLE  ─ on D5 → S_DATA_D5
S_DATA_D5    ─ on AA → S_DATA_AA
             ─ else  → S_DATA_IDLE (with D5 overlap as above)
S_DATA_AA    ─ on AD → S_DATA_BODY (set counter = 0)
             ─ else  → S_DATA_IDLE
S_DATA_BODY  ─ on each nibble: counter++; on DE peek-ahead 3:
                if next two are AA EB → fire OnDataMarkRead (cachedSector, 256),
                                       → S_DATA_IDLE
              ─ if counter exceeds (kDataNibbleCount + small slack):
                                       → S_DATA_IDLE (giving up; corrupt frame)
```

The "peek-ahead" is implemented by buffering the last 3 nibbles in a
3-byte rolling window inside the state, not by re-reading the nibble
stream (the watcher is forbidden from doing that per FR-008).

Both state machines run on every `ObserveNibble` call. They share no
mutable state with each other beyond the cached most-recent sector
number (written by the address-mark accept terminal, read by the
data-mark accept terminal).

### SPSC Ring Buffer

```cpp
class DiskIIEventRing {
    static constexpr uint32_t        kCapacity = 4096;  // power of 2
    static constexpr uint32_t        kMask = kCapacity - 1;
    DiskIIEvent                      m_slots[kCapacity];
    std::atomic<uint32_t>            m_head{0};         // consumer reads/writes
    std::atomic<uint32_t>            m_tail{0};         // producer reads/writes

public:
    bool TryPush (const DiskIIEvent& e);   // CPU thread only
    bool TryPop  (DiskIIEvent& out);       // UI thread only
    uint32_t Drain (DiskIIEvent* out, uint32_t maxCount);  // UI thread only
    uint32_t ApproxSize() const;           // either thread (advisory only)
};
```

`TryPush` is wait-free: load tail relaxed, load head acquire, check free
slot count, write event into `m_slots[tail & kMask]`, store tail release.
`TryPop` mirrors with roles reversed. `Drain` is the UI-thread fast path
that calls `TryPop` in a loop up to `maxCount` (or until the ring is
empty). On a 64-bit Windows host, `uint32_t` atomics are lock-free; the
ring carries no spurious lock fallback.

### Auto-Tail Scroll Algorithm

Implements FR-013 exactly:

```cpp
void DiskIIDebugDialog::OnDrainTimer()
{
    bool                wasAtTail;
    uint32_t            oldCount;
    uint32_t            dropped;

    if (m_paused || !IsWindowVisible (m_hwnd))
        return;

    oldCount   = (uint32_t) m_deque.size();
    wasAtTail  = (ListView_GetTopIndex (m_listView) + ListView_GetCountPerPage (m_listView)
                  >= (int) oldCount);

    dropped    = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);
    m_projection.DrainAndProject (m_ring, m_deque, dropped);

    if ((uint32_t) m_deque.size() == oldCount)
        return;    // nothing arrived; don't disturb scroll

    ListView_SetItemCountEx (m_listView, m_deque.size(), LVSICF_NOSCROLL);

    if (wasAtTail)
        ListView_EnsureVisible (m_listView, (int) m_deque.size() - 1, FALSE);
}
```

The `wasAtTail` capture is done **before** the projection mutates the
deque; this is the entire correctness condition for FR-013.

### Menu, Accelerator, Dialog Wire-Up

`Casso/resource.h` gains a new menu-item id: `IDM_VIEW_DISKII_DEBUG`.
`Casso/Casso.rc` gains:

- Under the existing `View` popup: a new "Disk II Debug...\tCtrl+Shift+D"
  item with the new id.
- In the application accelerator table: an `ACCELERATOR` entry binding
  `VK_D, IDM_VIEW_DISKII_DEBUG, VIRTKEY, CONTROL, SHIFT`.

`Casso/MenuSystem.{h,cpp}` routes `WM_COMMAND` with the new id to a
shell method `EmulatorShell::OpenDiskIIDebugDialog()`, which:

- If the dialog is not yet created: constructs it (programmatic
  `CreateWindowEx`-based dialog, no `.rc` template per A-008), attaches
  the dialog as the sink on `DiskIIController[0]`, starts the timer,
  shows the window.
- If the dialog already exists: brings it to front (`SetForegroundWindow`).

The dialog's `WM_CLOSE` handler hides the window and revokes the sink
(`controller.SetEventSink (nullptr)`); the dialog is reused on the next
open. The dialog is fully destroyed only on shell shutdown.

## Constitution Check — Post-Design Re-Validation

- ✅ All new functions stay within size and indent budgets.
- ✅ EHM applied only where fallibility exists (`CreateWindowEx`, accelerator
      registration); event-fire, ring push/pop, and watcher transitions
      are `void` and infallible.
- ✅ No new threads. Synchronization is exactly two `std::atomic<uint32_t>`
      indices on a lock-free SPSC ring + one `std::atomic<uint32_t>` lost-
      event counter; no mutex, no CV, no critical section.
- ✅ Test isolation: ring, watcher, and projection helpers are pure
      data structures testable without HWND/timer/filesystem; the dialog's
      Win32 wiring is intentionally thin.
- ✅ No external dependencies; nothing new in `vcpkg.json` or third-party
      headers.
- ✅ Sink pointer guarded at every fire site; controller behavior with
      no sink attached is byte-identical to pre-feature (SC-007, SC-010).

## Phasing (cross-reference to `tasks.md`)

The implementation follows the dependency order required by the design:
interface first, then producer-side pieces, then consumer-side pieces,
then the Win32 wiring, then polish. Phase summary:

1. **Phase 0 — Contracts**: `IDiskIIEventSink` interface, `DiskIIEvent`
   POD, `DiskIIEventDisplay` struct, named constants. Header-only.
2. **Phase 1 — SPSC ring**: `DiskIIEventRing.{h,cpp}` + exhaustive
   single-threaded and two-threaded tests (push fills, pop drains,
   wrap-around, overflow returns false without corruption).
3. **Phase 2 — Address-mark watcher**: `DiskIIAddressMarkWatcher.{h,cpp}`
   with both state machines + tests against synthetic nibble streams
   (stock DOS 3.3 cadence, bad checksum rejection, mid-stream resync,
   data-mark without preceding address mark, garbage stream produces
   zero false positives).
3. **Phase 3 — Controller integration**: `IDiskIIEventSink* m_eventSink` +
   `SetEventSink` + fire-site insertions at every existing motor/head/
   mount call site + watcher invocation in the nibble read path. Tests
   using a recording mock sink confirm event ordering and that the
   no-sink path is byte-identical to baseline.
4. **Phase 4 — UI-thread projection helper**: `DebugDialogProjection`
   (or equivalent) — the non-Win32 helper that drains the ring into the
   deque, formats per FR-005, applies the rolling cap, and inserts the
   `[N events lost]` marker. Pure data-structure tests.
5. **Phase 5 — Dialog skeleton**: `DiskIIDebugDialog.{h,cpp}` —
   programmatic `WM_INITDIALOG` that creates the ListView (virtual mode,
   4 columns), 5 filter checkboxes, Pause and Clear buttons. No event
   data yet; the dialog renders an empty list. Hooks `WM_DESTROY` /
   `WM_CLOSE`.
6. **Phase 6 — Drain timer + auto-tail**: `WM_TIMER` handler invokes the
   projection helper, captures `wasAtTail`, calls
   `ListView_SetItemCountEx`, conditionally `ListView_EnsureVisible`.
   `LVN_GETDISPINFO` handler returns deque entries by index.
7. **Phase 7 — Filter projection**: Wire the five filter checkboxes to a
   bitmask; on toggle, recompute the displayed index vector (or invalidate
   the LV) and re-render. Tests confirm chronological preservation when
   a filter is re-checked.
8. **Phase 8 — Pause / Resume / Clear**: Wire buttons. Pause stops the
   drain (events keep accumulating in the ring); Clear empties the deque
   but does not flush the ring. Tests confirm in-flight events survive
   Clear and the `[N events lost]` marker appears exactly once after
   any number of pause-induced overflows.
9. **Phase 9 — Clipboard copy**: Ctrl+C selection → tab-separated text
   on the clipboard (FR-019).
10. **Phase 10 — Menu / accelerator / shell wiring**: `Casso.rc`,
    `resource.h`, `MenuSystem.{h,cpp}`, `EmulatorShell` open/close
    handler with sink attach/revoke. Multi-controller indicator if
    `MachineConfig` reports > 1 Disk II.
11. **Phase 11 — Polish**: CHANGELOG, README, manual DOS 3.3 boot
    sanity, manual Karateka / Choplifter / HardHatMack inspection,
    final constitution sweep (`Pch.h` first, spacing, declarations
    aligned).

## Risks & Mitigations

| Risk                                                                                  | Mitigation                                                                                                                            |
|---------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| SPSC ring's correctness depends on memory-ordering discipline                         | Implement once, test exhaustively against a 2-thread stress harness using `_InterlockedIncrement` / `std::atomic` debug-assert peer; cross-check against Folly's `ProducerConsumerQueue` and Vyukov's classic SPSC formulation (research §4). |
| Address-mark watcher emits false positives on garbage nibbles                         | Checksum verification (FR-008 step 2) rejects every accidental `D5 AA 96` lookalike that doesn't have a matching XOR. Tests include a random-nibble torture set that asserts zero `OnAddressMark` calls. |
| Watcher perturbs the existing nibble engine                                            | Watcher is `const`-on-the-engine: it only reads what `NibbleReadByte` returns to the CPU. Existing `DiskIIControllerTests` must pass byte-identically with the sink-attached-but-stubbed configuration (SC-007). |
| ListView in virtual mode chokes on the row-count-update + ensure-visible cycle         | Virtual mode is specifically designed for this; AppleWin, x64dbg, and Process Explorer all use the same pattern at higher rates. 30 Hz updates with ≤ 4,096 new rows per tick is trivially within the control's documented envelope. |
| Cross-thread torn reads of `DiskIIEvent` fields                                       | The ring's release/acquire pairing on the index publish/observe is the standard SPSC happens-before edge. The slot data itself is plain-old-data — no torn reads possible because the publish is the index update, and the index update happens after the slot write on the producer side and before the slot read on the consumer side. |
| Filter checkbox toggle stalls UI on a 100,000-entry deque                              | Filter projection is computed lazily inside `LVN_GETDISPINFO` (only visible rows are touched); the recomputation on toggle is O(visible rows), not O(deque size). If a precomputed index vector is used, the rebuild is O(deque size) but happens on a user-driven event, not in the drain timer. |
| Dialog opened mid-session shows no historical context                                  | This is by design (FR-018). README and CHANGELOG mention "open the debug window *before* the operation you want to investigate." |
| Multi-controller machine config in the future                                          | v1 surfaces a clear indicator and monitors controller #0 only. The interface and ring are per-window so a future v2 can host one window per controller without changing the underlying contract. |
