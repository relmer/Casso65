# Implementation Plan: Disk II Audio

**Branch**: `feature/005-disk-ii-audio` | **Date**: 2026-03-19 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/005-disk-ii-audio/spec.md`
**Research basis**: [`research.md`](./research.md) — source-verified survey of OpenEmulator, MAME, AppleWin, and the Casso codebase
**Constitution**: `.specify/memory/constitution.md`

## Summary

Add realistic Disk II mechanical audio (motor hum, head-step clicks,
track-0 bumps, disk insert/eject) to Casso's //e emulator and mix it
into the existing WASAPI pipeline alongside the speaker, in stereo with
per-drive panning. The implementation is structured around a new
`DriveAudioMixer` that owns a collection of `IDriveAudioSource` instances
(one per attached drive). v1 ships one concrete source — `DiskIIAudioSource`
— driven by an `IDriveAudioSink` event interface that the Disk II
controller fires at the precise points where head, motor, and disk-mount
state changes occur. The mixer is consumed once per audio frame by
`WasapiAudio::SubmitFrame()`, which mixes drive PCM additively (stereo,
per-channel) into the same float buffer that the speaker writes (speaker
is centered, drives are panned). A "Drive Audio" check toggle in a new
**View → Options...** dialog (default on) gates the mixer.

The abstraction is intentionally generic: future drive types (//c internal
5.25, DuoDisk, Apple 5.25 Drive, Apple /// drive, ProFile) implement
`IDriveAudioSource` and register with the same mixer; the mixer, sink
interface, and WASAPI plumbing remain untouched.

The design is grounded in `research.md` §4–5 (Casso-specific implementation
sketch), which fingerprints exact line ranges in
`DiskIIController.{h,cpp}`, `WasapiAudio.{h,cpp}`, and `EmulatorShell.cpp`
where touch points are required.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK + STL only — Windows MediaFoundation
(`IMFSourceReader`) for WAV decoding, or in-repo minimal PCM WAV parser.
No new third-party libraries.
**Storage**: `Assets/Sounds/DiskII/*.wav` if bundled samples are chosen
(PascalCase filenames: `MotorLoop.wav`, `HeadStep.wav`, `HeadStop.wav`,
`DoorOpen.wav`, `DoorClose.wav`); otherwise procedural synthesis at startup.
Real-hardware recordings under permissive license preferred over synthesis
(NFR-005).
**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New
`DriveAudioMixerTests.cpp`, `DiskIIAudioSourceTests.cpp`, plus extensions
to existing `DiskIIControllerTests.cpp` to cover the new event firing
(including disk insert/eject).
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Existing 5-project solution; new code in `CassoEmuCore`
(library) and `Casso` (GUI shell). No new project.
**Performance Goals**: Adding drive-audio mixing MUST NOT measurably
increase per-frame WASAPI submit cost; the inner loop is a small fixed
number of float multiply-adds per active sample stream per channel (≤ 6
simultaneous streams in v1: motor + head + door per drive, up to 2 drives).
**Constraints**: No regression in speaker pipeline (FR-011, SC-006). No
buffer underruns (NFR-001). Same-thread state model — no locks introduced
in v1 (NFR-002, with explicit revisit point if expensive DSP arrives).
Asset footprint is advisory (NFR-003, revisit at >1 MB compressed).
GPL-3 drive-audio samples MUST NOT be committed (NFR-004). Real-hardware
recordings preferred over synthesis when permissively licensed (NFR-005).
**Scale/Scope**: ~5 new source files (~700–900 LOC), ~6 touched files,
~25–40 new test cases. New Options dialog scaffold (small).

## Constitution Check

### Principle I — Code Quality (NON-NEGOTIABLE)

| Rule                                              | How this plan complies                                                                                                                  |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| Pch.h-first include                               | Every new `.cpp` includes `"Pch.h"` as the first include. No angle-bracket includes outside `Pch.h`. All STL/Windows headers go via `Pch.h`. |
| Formatting Preservation                           | All edits to existing files are surgical insertions at the line ranges named in research §5.3; no existing aligned blocks are touched.  |
| EHM on fallible functions                         | `DiskIIAudioSource::LoadSamples`, WAV-decode helpers, and any MediaFoundation calls use the standard `HRESULT hr; … Error: … return hr;` pattern. |
| No calls inside macro arguments                   | Store results first; reviewed per PR.                                                                                                   |
| Single exit via `Error:` label                    | Standard pattern in every new fallible function.                                                                                        |
| Avoid Nesting (≤ 2-3 indent beyond EHM)           | `GeneratePCM()` factored into `MixMotor()`, `MixHead()`, `MixDoor()` helpers, each ≤ 2 levels.                                          |
| Variable Declarations at Top of Scope             | All new functions follow this; column-aligned per project rules.                                                                        |
| Function Comments in `.cpp` Only                  | Headers carry only declaration comments; doc blocks live in `.cpp` with 80-`/` delimiters.                                              |
| Function Spacing — `func()` vs `func (a, b)`      | Verified via `rg -n '\w \(\)' CassoEmuCore/Audio/ Casso/` before commit on every PR phase.                                              |
| Smart Pointers                                    | `DriveAudioMixer` is owned by `EmulatorShell` directly (composition); no smart pointer needed. Sample buffers are `std::vector<float>`. |
| PascalCase file names                             | All new source files (`DriveAudioMixer.{h,cpp}`, `DiskIIAudioSource.{h,cpp}`, `IDriveAudioSink.h`, `IDriveAudioSource.h`, `OptionsDialog.{h,cpp}`) and asset files (`MotorLoop.wav`, `HeadStep.wav`, `HeadStop.wav`, `DoorOpen.wav`, `DoorClose.wav`) use PascalCase with no underscores. |
| No magic numbers                                  | `kMotorVolume = 0.25f`, `kHeadVolume = 0.30f`, `kDoorVolume = 0.30f`, `kSpeakerCenter = 0.7071f` (= √0.5), `kDrivePanOffset = 0.3927f` (= π/8 radians), `kSeekThresholdCycles = 16368`, `kHeadIdleCycles = 51150` are all named constants. |

### Principle II — Test Isolation (NON-NEGOTIABLE)

Drive-audio tests run on the same headless harness as existing
`CassoEmuCore` tests. `DriveAudioMixer` and `DiskIIAudioSource` are
constructed with caller-owned in-memory sample buffers; tests never read
host filesystem. The `IDriveAudioSink` interface lets `DiskIIController`
tests substitute a recording mock with no audio device involved.

### Principle V — Function Size & Structure

`GeneratePCM()` is the only function with appreciable logic. `DriveAudioMixer::GeneratePCM` factors as:

```
GeneratePCM (stereoOut, n)
  Clear (stereoOut, 2*n)
  if !m_enabled: return
  for each registered source:
      source.GeneratePCM (sourceMono, n)           // ≤ 25 LOC inside source
      ApplyPanAndMix (stereoOut, sourceMono, n,    // ≤ 10 LOC
                      source.PanLeft, source.PanRight)
  // No motor-hum dedup — see FR-008 (independent per-drive sum,
  // bounded by equal-power panning + phase incoherence).
```

`DiskIIAudioSource::GeneratePCM` (mono output):
```
GeneratePCM (out, n)
  MixMotor (out, n)                                // ≤ 15 LOC
  MixHead  (out, n)                                // ≤ 20 LOC
  MixDoor  (out, n)                                // ≤ 15 LOC
  UpdateSeekState (currentCycle)                   // ≤ 10 LOC
```

All helpers stay under ~25 LOC.

## Architecture

### Components

```
                 ┌─────────────────────┐
                 │  DiskIIController   │  (CassoEmuCore/Devices)
                 │   ─ HandleSwitch    │──┐
                 │   ─ HandlePhase     │  │ IDriveAudioSink*
                 │   ─ Tick (spindown) │  │
                 │   ─ NotifyInsert    │  │ OnMotorStart()
                 │   ─ NotifyEject     │  │ OnMotorStop()
                 └─────────────────────┘  │ OnHeadStep(qt)
                                          │ OnHeadBump()
                                          │ OnDiskInserted()
                                          │ OnDiskEjected()
                                          ▼
                 ┌──────────────────────────────────────────┐
                 │ DiskIIAudioSource : IDriveAudioSource    │
                 │                     : IDriveAudioSink    │ (CassoEmuCore/Audio)
                 │   ─ LoadSamples / SynthesizeSamples      │
                 │   ─ panLeft, panRight (per-drive)        │
                 │   ─ GeneratePCM (out, n)  → mono floats  │
                 │      ├── MixMotor                        │
                 │      ├── MixHead (step + bump + seek)    │
                 │      └── MixDoor (open/close one-shots)  │
                 └──────────────────────────────────────────┘
                            ▲                       │
                            │ registers             │ mono PCM
                            │                       ▼
                 ┌──────────┴─────────────────────────────────┐
                 │ DriveAudioMixer                            │  (CassoEmuCore/Audio)
                 │   ─ RegisterSource (IDriveAudioSource*)    │
                 │   ─ SetEnabled                             │
                 │   ─ GeneratePCM (stereoOut, n)             │◄── called from WasapiAudio
                 │      └── for each source: pan+sum→stereo   │
                 │          (no dedup; FR-008, FR-012)        │
                 └────────────────────────────────────────────┘
                            ▲                       │
                            │ owns                  │ stereo float* diskBuf
                            │                       ▼
                 ┌──────────┴───────────┐   ┌──────────────────────────┐
                 │   EmulatorShell      │──►│   WasapiAudio            │
                 │   ─ owns mixer       │   │   ─ SubmitFrame (..., mix)│
                 │   ─ owns Options dlg │   │     ├ GeneratePCM(speaker)│
                 │   ─ owns Disk II     │   │     │   → center stereo  │
                 │     audio sources    │   │     ├ GeneratePCM(mix)   │
                 │   ─ SetAudioSink     │   │     │   → stereo         │
                 │     (controller →    │   │     └ additive mix +     │
                 │      source)         │   │       clamp per channel  │
                 │   ─ NotifyInsert/    │   └──────────────────────────┘
                 │     Eject from UI    │
                 └──────────────────────┘
                            ▲
                            │ View menu  → Options... → "Drive Audio" check
                            │             → mixer.SetEnabled (checked)
                            ▼
                 ┌──────────────────────┐
                 │   OptionsDialog      │  (Casso/OptionsDialog.{h,cpp})
                 │   ─ Drive Audio chk  │
                 │   ─ (future toggles) │
                 └──────────────────────┘
```

### Data Flow Per Audio Frame

1. CPU runs an emulation slice (`ExecuteCpuSlices()` in `EmulatorShell.cpp`).
2. Within the slice, `DiskIIController::HandleSwitch()`, `HandlePhase()`, and
   `Tick()` may fire `OnMotorStart` / `OnMotorStop` / `OnHeadStep` /
   `OnHeadBump` into the drive's `DiskIIAudioSource`. UI mount/eject paths
   fire `OnDiskInserted` / `OnDiskEjected`. These calls mutate
   `m_motorRunning`, `m_headBuf`/`m_headPos`, `m_doorBuf`/`m_doorPos`,
   `m_lastStepCycle`, `m_seekMode`.
3. After the slice, `WasapiAudio::SubmitFrame()` is called with the mixer
   pointer. It generates `numSamples` of speaker PCM into a scratch buffer
   (mono → duplicated to L=R for stereo), then asks the mixer to generate
   `numSamples * 2` of stereo drive PCM into a parallel stereo scratch
   buffer, then sums per-channel (clamping to `[-1, +1]`) into
   `m_pendingSamples`.
4. WASAPI drains `m_pendingSamples` to the render client (downmixing to mono
   if the device is mono).

No new threads; no new locks. All mutation and consumption happen on the
single CPU-emulation thread that owns `ExecuteCpuSlices()`.

### Event Trigger Specification

Reproduced from `research.md` §4.1, normative for this feature:

| Event                | Trigger                                                                         | Call site                                    |
|----------------------|----------------------------------------------------------------------------------|----------------------------------------------|
| `OnMotorStart`       | `m_motorOn` transitions from false → true                                        | `HandleSwitch()` case 0x9                    |
| `OnMotorStop`        | `m_motorOn` transitions from true → false (post-spindown)                        | `Tick()` after spindown counter expires      |
| `OnHeadStep(newQt)`  | `qtDelta != 0` AND head NOT clamped against a stop                               | `HandlePhase()` after `m_quarterTrack += qtDelta` |
| `OnHeadBump()`       | `qtDelta != 0` AND post-clamp `m_quarterTrack == 0` (from < 0) OR `== kMaxQuarterTrack` (from > kMaxQuarterTrack) | Same `HandlePhase()` site |
| `OnDiskInserted()`   | Disk image newly mounted (or swapped onto a previously-mounted drive after eject) | `DiskIIController::MountImage` (or shell-level mount path) |
| `OnDiskEjected()`    | Disk image unmounted                                                             | `DiskIIController::EjectImage` (or shell-level eject path) |

`OnHeadStep` and `OnHeadBump` are mutually exclusive for any single
`HandlePhase()` invocation. Door events are independent of motor state.

### Step-vs-Seek Discrimination (FR-005)

The mixer adopts the **OpenEmulator cycle-gap approach** (per `research.md`
§4.3) translated to Casso's 1.023 MHz cycle counter:

- `kSeekThresholdCycles = 16368` (≈ 16 ms × 1.023 MHz)
- On `OnHeadStep`: if `(currentCycle - m_lastStepCycle) < kSeekThresholdCycles`,
  enter seek mode: do **not** restart the `HeadStep` one-shot; instead, if
  not already in seek mode, start (or keep playing) a continuous seek-rate
  audio. Otherwise (gap ≥ threshold), restart the `HeadStep` one-shot.
- `m_lastStepCycle` is updated on every `OnHeadStep`.
- Seek mode auto-clears when no step arrives within `kHeadIdleCycles = 51150`
  (≈ 50 ms). Checked at the top of `GeneratePCM()`.

The "continuous seek" sound in v1 may be implemented either as a separate
loop sample or simply by holding the `HeadStep` sample's tail / not
restarting it; the spec (FR-005) requires that the audible result not
fragment into N discrete clicks.The exact synthesis is an implementer's
choice within FR-005.

### Sample Sourcing (Implementation Choice, Per Spec)

Two implementations satisfy the spec:

- **Bundled WAVs**: `Assets/Sounds/DiskII/{MotorLoop,HeadStep,HeadStop,DoorOpen,DoorClose}.wav`.
  Decoded once at startup via `IMFSourceReader` to mono float32 at the
  WASAPI mix rate, owned by `DiskIIAudioSource`. Real Disk II recordings
  under permissive license preferred (NFR-005).
- **Procedural synthesis**: At `DiskIIAudioSource` startup, fill internal
  buffers with synthesized PCM (motor: low-frequency sawtooth + noise; head
  click: short attack + Karplus-Strong-style decay; bump: same but
  longer/lower; door open/close: ratchet/snap synthesis).

Either way, the downstream contract (`GeneratePCM`, event hooks, source
state machine) is identical. `LoadSamples` returns `S_OK` if anything
loaded successfully, and the source simply mutes any sound whose buffer is
empty (FR-009, graceful degradation).

### Options Dialog Wire-Up

`Casso/OptionsDialog.{h,cpp}` (new) hosts the "Drive Audio" check toggle.
`Casso/MenuSystem.{h,cpp}` gains:

- A new menu-item id `IDM_VIEW_OPTIONS`
- Under the existing `View` popup: a new "Options..." item
- Handler: opens `OptionsDialog`, which on OK applies settings (e.g.,
  `m_driveAudioMixer.SetEnabled(checked)`).

Default state on first launch: "Drive Audio" checked (FR-006). No
persistence required for v1; if a persistence mechanism exists or is
added, the dialog SHOULD use it.

### Cold-Boot Mount Suppression (FR-013)

Per FR-013, disk insert sounds fire only for **user-initiated, mid-session**
mounts. Cold-boot mounts (command-line arguments, last-session restoration,
autoload) MUST NOT fire `OnDiskInserted()`. `EmulatorShell` tracks a
"startup phase" flag that begins `true` and transitions to `false` after
machine initialization completes and the main message loop begins
delivering user input; any mount performed while the flag is `true`
suppresses the insert event. Eject events always fire (no cold-boot eject
case in practice). Implementation lands in T082b; paired tests in T082c.

### Mixing Math (FR-010, FR-011, FR-012, NFR-001)

Stereo, equal-power pan (constant-loudness across positions), per-channel
clamp:

```
// Speaker is centered using equal-power center: L = R = sqrt(0.5) ≈ 0.707
for i in [0, n):
    speakerL[i] = speaker[i] * kSpeakerCenter   // kSpeakerCenter = 0.7071
    speakerR[i] = speaker[i] * kSpeakerCenter

// Each drive source produces mono, panned per-drive (equal-power):
for each source s with precomputed panL, panR (panL² + panR² = 1):
    s.GeneratePCM (srcMono, n)
    for i in [0, n):
        diskL[i] += srcMono[i] * s.panLeft
        diskR[i] += srcMono[i] * s.panRight

// Sum and clamp per channel:
for i in [0, n):
    outL[i] = clamp (speakerL[i] + diskL[i], -1.0f, +1.0f)
    outR[i] = clamp (speakerR[i] + diskR[i], -1.0f, +1.0f)
```

Per-source pan (FR-012, equal-power, computed once in `SetPan`):

Pan position is parameterized by `kDrivePanOffset = π/8 ≈ 0.3927` (radians).
Drive angles are measured from the right speaker, so `π/4` is center, `π/2`
is hard left, and `0` is hard right. The constant controls the magnitude
of the offset from center.

- Drive 1 (left bias): `θ = π/4 + kDrivePanOffset = 3π/8`
  - `panL = cos(θ) ≈ 0.924`, `panR = sin(θ) ≈ 0.383`
- Drive 2 (right bias): `θ = π/4 - kDrivePanOffset = π/8`
  - `panL = cos(θ) ≈ 0.383`, `panR = sin(θ) ≈ 0.924`
- Centered (lone drive, or future explicitly-centered source): `θ = π/4`
  - `panL = panR = √0.5 ≈ 0.707`

`SetPan(float panLeft, float panRight)` stores the precomputed coefficients
directly; callers that want angle-based placement use a helper like
`SetPanAngle(float thetaRadians)` that does the `sin`/`cos` once.

Per-sound attenuation (source-internal):

- Motor loop: ×0.25
- Head one-shot: ×0.30
- Door one-shot: ×0.30
- Speaker (unchanged): ×0.50 (existing `AudioGenerator` behavior)

Worst-case per-channel sum: single-drive full speaker + full motor + full
head, drive centered (`0.707` pan):
`0.50 × 0.707 + (0.25 + 0.30) × 0.707 ≈ 0.74` per channel — safely under
1.0. Two-drive case is bounded by per-drive panning concentrating each
drive's energy in one channel; see FR-008 rationale.

When the WASAPI device is mono, the stereo output is downmixed by
`(L + R) × 0.5` after clamp, preserving overall amplitude.

## Constitution Check — Post-Design Re-Validation

- ✅ All new functions stay within size and indent budgets.
- ✅ EHM applied to `LoadSamples` and WAV-decode path; `GeneratePCM`,
      `MixMotor`, `MixHead`, and event hooks are `void` and infallible
      (they only mutate POD state and float buffers).
- ✅ No new threads, no locks (NFR-002).
- ✅ No GPL-3 sample assets committed (NFR-004) — the asset directory is
      either populated by the implementer with self-recorded or synthesized
      content, or empty (graceful-degradation path takes over).
- ✅ Test isolation: in-memory sample buffers, mock sink for controller
      tests, no host filesystem reads.

## Phasing (cross-reference to `tasks.md`)

The implementation follows `research.md` §6.3's recommended order, expanded
into atomic tasks in [`tasks.md`](./tasks.md). Phase summary:

1. **Phase 0 — Contracts**: `IDriveAudioSink`, `IDriveAudioSource`,
   `DriveAudioMixer.h`, `DiskIIAudioSource.h` declarations, named constants.
2. **Phase 1 — Source skeleton**: `DiskIIAudioSource.cpp` with silent
   `GeneratePCM`, infallible event hooks (incl. insert/eject), unit tests
   against a mock that exercises every event combination.
3. **Phase 2 — Mixer skeleton**: `DriveAudioMixer.cpp` with source
   registration, stereo equal-power pan+sum (no motor-hum dedup per
   revised FR-008), silent-source tests.
4. **Phase 3 — Controller wiring**: Add `IDriveAudioSink* m_audioSink` to
   `DiskIIController.h`; add 4 emulation call sites plus mount/eject calls.
   Tests using a recording sink confirm event ordering.
5. **Phase 4 — Sample loading / synthesis**: Implement `LoadSamples` or
   `SynthesizeSamples` in `DiskIIAudioSource`. Real-recording preference
   documented per NFR-005.
6. **Phase 5 — Mixer playback**: `MixMotor`, `MixHead`, `MixDoor`. Tests
   verify per-sample output for canonical sequences.
7. **Phase 6 — Step/seek discrimination**: `m_lastStepCycle`, `m_seekMode`,
   idle timeout; tests verify rapid-step bursts collapse per FR-005.
8. **Phase 7 — WASAPI stereo integration**: Negotiate stereo from WASAPI,
   extend `SubmitFrame` to produce stereo, additive per-channel mix,
   per-channel clamp, mono device downmix. Speaker regression test (SC-006).
9. **Phase 8 — Shell wiring**: `EmulatorShell` owns mixer + per-drive
   sources; sink hookup; mount/eject hookup. Boot-and-listen sanity in
   DOS 3.3 fixture (manual).
10. **Phase 9 — Options dialog**: Create `OptionsDialog.{h,cpp}`; add
    View → Options... menu entry; Drive Audio checkbox; runtime toggle test.
11. **Phase 10 — Asset graceful-degradation**: Verify missing-file path;
    one non-fatal warning per missing asset; no popup.
12. **Phase 11 — Polish**: CHANGELOG, README, manual A/B listening, final
    constitution sweep.

## Risks & Mitigations

| Risk                                                                                  | Mitigation                                                                                                                            |
|---------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| Step-vs-seek tuning sounds wrong for non-DOS-3.3 software (ProDOS, copy-protected)    | Threshold is a single named constant (`kSeekThresholdCycles`); tune via listening test against the existing fixture disks in Phase 4. |
| Disk-audio amplitude causes audible clipping when speaker is at full deflection        | Per-source attenuation + post-sum clamp; tune `kMotorVolume`/`kHeadVolume` in Phase 5 with the speaker test suite as regression guard.|
| Missing or broken WAV files at user installs                                          | FR-009 graceful degradation; the mixer treats any empty buffer as "muted." Verified in Phase 8.                                       |
| `IMFSourceReader` initialization failure on locked-down systems                       | WAV-decode path returns HRESULT; on failure, the affected sample buffer stays empty and FR-009 kicks in. No popup, single log line.   |
| GPL-3 sample contamination from development snapshots                                 | `.gitignore` does **not** silence `Assets/Sounds/DiskII/` (per project hygiene rules); stray files surface in `git status`. PR review responsibility. |
| Mixing introduces buffer-underrun (NFR-001)                                           | Disk mix is pure CPU float math inside the same `SubmitFrame` call; no I/O, no syscalls. Measure WASAPI underrun count in soak test.  |
