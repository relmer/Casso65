# Feature Specification: Apple II Platform Emulator

**Feature Branch**: `003-apple2-platform-emulator`  
**Created**: 2025-07-22  
**Status**: Draft  
**Input**: User description: "Add a GUI-based Apple II platform emulator (Casso65Emu) to the Casso65 6502 emulator project"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot Apple II+ to BASIC Prompt (Priority: P1)

A user launches Casso65Emu with an Apple II+ machine configuration and the appropriate ROM image. The emulator window opens, the CPU executes the ROM boot sequence, and the familiar 40-column text display appears with the Applesoft BASIC `]` prompt. The user can type BASIC commands using their PC keyboard and see the output on screen.

**Why this priority**: This is the foundational "first visible output" that proves the entire architecture works end-to-end — CPU execution, memory bus routing, ROM loading, keyboard input, text video rendering, and Win32 display. Every subsequent feature builds on this.

**Independent Test**: Can be fully tested by launching `Casso65Emu.exe --machine apple2plus` with a valid ROM file and verifying the `]` prompt appears. The user can type `PRINT "HELLO"` and see the output. Delivers a working text-mode Apple II+ emulator.

**Acceptance Scenarios**:

1. **Given** a valid Apple II+ ROM file is present in the configured ROM directory, **When** the user runs `Casso65Emu.exe --machine apple2plus`, **Then** a window opens displaying 40-column green-on-black text with the Applesoft BASIC `]` prompt within 3 seconds
2. **Given** the emulator is running at the `]` prompt, **When** the user types `PRINT "HELLO WORLD"` and presses Enter, **Then** the text `HELLO WORLD` appears on the next line followed by a new `]` prompt
3. **Given** the emulator is running, **When** the user presses Ctrl+Reset, **Then** the emulator resets and returns to the `]` prompt
4. **Given** the machine config specifies a CPU clock speed of 1.023 MHz, **When** the emulator runs, **Then** the emulation speed closely approximates real Apple II timing (cursor blink rate and keystroke responsiveness feel authentic)

---

### User Story 2 - Speaker Audio Output (Priority: P2)

A user runs a BASIC program that produces sound (e.g., `FOR I=1 TO 100: S=PEEK(49200): NEXT`) and hears audio output through their PC speakers. The speaker toggle at $C030 produces square-wave audio at the correct pitch and duration.

**Why this priority**: Sound provides immediate user feedback and demonstrates the soft-switch framework is working correctly. It is a small, self-contained feature that enhances the experience significantly.

**Independent Test**: Can be tested by running a BASIC program that toggles the speaker address and verifying audible output. Delivers audio capability to the text-mode emulator.

**Acceptance Scenarios**:

1. **Given** the emulator is running at the BASIC prompt, **When** the user runs a program that reads address $C030 repeatedly, **Then** audible sound is produced through the PC speakers
2. **Given** the speaker is producing sound, **When** the toggle rate changes, **Then** the audio pitch changes correspondingly

---

### User Story 3 - Lo-Res Graphics Display (Priority: P3)

A user runs a BASIC program that uses lo-res graphics commands (`GR`, `COLOR=`, `PLOT`, `HLIN`, `VLIN`) and sees a 40×48 grid of colored blocks on the emulator display. All 16 Apple II colors render correctly.

**Why this priority**: Lo-res graphics is the simplest graphics mode, sharing memory with the text page. It proves the video mode switching (soft switches $C050–$C053) works and establishes the pattern for hi-res mode later.

**Independent Test**: Can be tested by entering `GR` at the BASIC prompt and running plotting commands. Delivers lo-res graphics capability.

**Acceptance Scenarios**:

1. **Given** the emulator is at the BASIC prompt, **When** the user types `GR`, **Then** the display switches to lo-res graphics mode with a 4-line text window at the bottom
2. **Given** lo-res mode is active, **When** the user runs `COLOR=1: PLOT 0,0`, **Then** a magenta block appears at the top-left corner
3. **Given** lo-res mode is active, **When** the user draws with all 16 color values (0–15), **Then** each color is visually distinct and matches standard Apple II colors

---

### User Story 4 - Language Card and DOS/ProDOS Loading (Priority: P4)

A user boots with a disk image containing DOS 3.3 or ProDOS. The Language Card bank-switching logic allows the operating system to load into the upper 16KB of RAM, and the `CATALOG` command lists files on the disk.

**Why this priority**: Bank-switching and disk access unlock the ability to run real Apple II software, which is the primary use case for most emulator users. This transforms the emulator from a BASIC interpreter into a platform that runs historical software.

**Independent Test**: Can be tested by launching with `--disk1 dos33.dsk` and verifying that DOS boots, then running `CATALOG` to list disk contents.

**Acceptance Scenarios**:

1. **Given** a DOS 3.3 system disk image, **When** the user runs `Casso65Emu.exe --machine apple2plus --disk1 dos33.dsk`, **Then** DOS 3.3 boots and displays the `]` prompt with the DOS greeting
2. **Given** DOS is running, **When** the user types `CATALOG`, **Then** the files on the disk image are listed correctly
3. **Given** a .dsk file with a BASIC program, **When** the user types `RUN PROGRAM`, **Then** the program loads from disk and executes
4. **Given** a write-protected disk image, **When** a program attempts to write to disk, **Then** the emulator reports a disk write-protection error consistent with real Apple II behavior

---

### User Story 5 - Hi-Res Graphics with Color (Priority: P5)

A user runs a program that uses hi-res graphics (e.g., `HGR`, `HPLOT`, or a game loaded from disk). The 280×192 display renders with NTSC color artifacting simulation, producing the characteristic Apple II color fringes.

**Why this priority**: Hi-res graphics are essential for running most Apple II games and productivity software. NTSC color artifacting is a defining visual characteristic of the platform.

**Independent Test**: Can be tested by entering `HGR` and using `HPLOT` commands or loading a hi-res image from disk.

**Acceptance Scenarios**:

1. **Given** the emulator is at the BASIC prompt, **When** the user types `HGR`, **Then** the display switches to hi-res mode showing a 280×192 graphics area with 4 lines of text at the bottom
2. **Given** hi-res mode is active, **When** a program draws alternating pixel patterns, **Then** NTSC color artifacting produces visible color fringes consistent with real Apple II output
3. **Given** hi-res mode is active, **When** the palette bit (bit 7) is toggled in a byte, **Then** the color set shifts between violet/green and blue/orange groups

---

### User Story 6 - Apple IIe Extended Features (Priority: P6)

A user selects the Apple IIe machine configuration and gets access to 128KB RAM, 80-column text mode, double hi-res graphics, lowercase keyboard input, and Open/Closed Apple keys. Programs that require IIe-specific features run correctly.

**Why this priority**: The Apple IIe was the most popular model and is needed to run a large portion of the Apple II software library. It demonstrates the extensibility of the data-driven machine configuration architecture.

**Independent Test**: Can be tested by launching with `--machine apple2e`, verifying 80-column text mode with `PR#3`, and testing lowercase input.

**Acceptance Scenarios**:

1. **Given** the Apple IIe machine config, **When** the user runs `Casso65Emu.exe --machine apple2e`, **Then** the emulator boots with the Apple IIe ROM and supports lowercase keyboard input by default
2. **Given** IIe mode, **When** the user activates 80-column mode (e.g., `PR#3`), **Then** the display switches to 80×24 character mode
3. **Given** IIe mode with 80-column active, **When** a program uses double hi-res graphics, **Then** 560×192 resolution graphics render with 16 colors
4. **Given** IIe mode, **When** the user presses the keys mapped to Open Apple and Closed Apple, **Then** the corresponding button state is readable by software at the standard addresses

---

### User Story 7 - Data-Driven Machine Configuration (Priority: P7)

A developer creates a new machine configuration JSON file and registers new device components in the factory registry. The emulator loads the new configuration without any changes to the core emulator shell or existing machine configs.

**Why this priority**: The extensible architecture is a core design goal, enabling future machines (C64, NES, etc.) to be added. While it's tested implicitly by the Apple II configurations, an explicit validation of the plug-in architecture ensures maintainability.

**Independent Test**: Can be tested by creating a minimal custom machine config JSON with RAM, ROM, and a text display, and verifying it loads and runs.

**Acceptance Scenarios**:

1. **Given** a valid machine config JSON file with a unique machine name, **When** the user references it via `--machine` on the command line, **Then** the emulator loads the configuration and instantiates the specified devices
2. **Given** a machine config references a device type name that exists in the component registry, **When** the emulator initializes, **Then** the device is created and wired to the memory bus at the configured address range
3. **Given** a machine config references a device type name that does not exist in the registry, **When** the emulator initializes, **Then** a clear error message names the unknown device type and the emulator exits gracefully

---

### Edge Cases

- What happens when the user specifies `--machine` with a config name that does not exist? → The emulator displays a helpful error listing available machine configurations and exits
- What happens when the ROM file referenced in the machine config is missing? → The emulator reports which ROM file is missing, its expected location, and exits with a clear error message
- What happens when the user provides a .dsk image that is corrupted or an unrecognized format? → The emulator reports the disk error and continues running (the drive is simply empty)
- What happens when the emulator window is resized? → The display scales proportionally, maintaining the correct aspect ratio, or the window has a fixed size
- What happens when the user closes the emulator window while a disk write is in progress? → The emulator completes or discards the pending operation and exits cleanly without corrupting the disk image
- What happens when two devices are configured with overlapping address ranges? → The emulator detects the conflict at startup and reports which devices and addresses overlap, then exits

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a separate GUI executable (`Casso65Emu`) that links against the existing Casso65Core static library without modifying Casso65Core's public API
- **FR-002**: System MUST load machine configurations from JSON files that define memory map, ROM file paths, CPU type, clock speed, device wiring, video geometry, and keyboard type
- **FR-003**: System MUST implement a component registry where named device types are registered as C++ classes and referenced by name in machine config files
- **FR-004**: System MUST route all CPU memory access through a MemoryBus that delegates reads and writes to the appropriate MemoryDevice based on address
- **FR-005**: System MUST implement RAM and ROM as MemoryDevice components that can be placed at configurable address ranges
- **FR-006**: System MUST implement Apple II soft switches as MemoryDevice components that respond to reads/writes at their configured addresses to toggle video modes, read keyboard state, and control other hardware functions
- **FR-007**: System MUST render Apple II text mode (40×24 characters) with the correct interleaved memory layout ($0400–$07FF) including the non-sequential row addressing
- **FR-008**: System MUST render Apple II lo-res graphics mode (40×48, 16 colors) from the text page memory, correctly decoding each byte as two 4-bit color nybbles
- **FR-009**: System MUST render Apple II hi-res graphics mode (280×192) from $2000–$3FFF, decoding 7 pixels per byte with palette bit control and simulating NTSC color artifacting
- **FR-010**: System MUST display the video output in a Win32 window using GDI framebuffer blitting, at a resolution of 560×384 (2× scaling of 280×192)
- **FR-011**: System MUST translate PC keyboard input to Apple II keyboard codes and make them available through the keyboard soft switch at $C000, with the strobe cleared on read of $C010
- **FR-012**: System MUST produce audio output by toggling the speaker state on reads of $C030, generating a square-wave signal through the Windows audio system
- **FR-013**: System MUST implement Language Card bank-switching (soft switches $C080–$C08F) to support 16KB of switchable RAM in the $D000–$FFFF address range
- **FR-014**: System MUST emulate the Disk II controller in slot 6, supporting reading and writing .dsk (DOS-order 140KB) disk images
- **FR-015**: System MUST support Apple IIe extensions including 128KB memory (64KB auxiliary RAM), 80-column text mode, double hi-res graphics, lowercase keyboard, and Open/Closed Apple keys — all configured through the Apple IIe machine config file
- **FR-016**: System MUST accept command-line arguments: `--machine <name>` (required), `--disk1 <path>` (optional), `--disk2 <path>` (optional)
- **FR-017**: System MUST NOT depend on any third-party libraries; only the Windows SDK and C++ Standard Library are permitted
- **FR-018**: System MUST validate machine config files at startup and report clear, actionable errors for missing files, unknown device types, overlapping address ranges, and malformed JSON
- **FR-019**: System MUST preserve all existing Casso65 project functionality — the existing 577+ unit tests must continue to pass with no changes to Casso65Core's public API

### Key Entities

- **MachineConfig**: Represents a complete machine definition loaded from JSON — includes CPU type, clock speed, memory layout, device list, video configuration, and keyboard type
- **MemoryBus**: Central address-space router that maps address ranges to MemoryDevice instances; handles read/write dispatch and address conflict detection
- **MemoryDevice**: Interface for any component on the address bus (RAM, ROM, soft switches, slot cards) — supports Read(address) and Write(address, value) operations
- **ComponentRegistry**: Factory registry mapping string device type names to C++ class constructors; used by the machine config loader to instantiate devices
- **VideoOutput**: Interface for video renderers that read video RAM and produce RGBA pixel framebuffers; each video mode (text, lo-res, hi-res, double hi-res) is a separate implementation
- **DiskImage**: Represents a mounted .dsk file with sector read/write capability; manages file I/O and protects against corruption on unexpected exit

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can boot an Apple II+ emulation to the Applesoft BASIC prompt and interact with it within 3 seconds of launching the application
- **SC-002**: Users can type and execute BASIC programs with keyboard response latency indistinguishable from a real Apple II (under 50 milliseconds perceived delay)
- **SC-003**: Users can boot DOS 3.3 or ProDOS from a .dsk image and run CATALOG, LOAD, and RUN commands successfully
- **SC-004**: All 16 Apple II lo-res colors display correctly and are visually distinguishable on a modern monitor
- **SC-005**: Hi-res graphics programs produce color output consistent with NTSC artifacting behavior documented in Apple II technical references
- **SC-006**: Emulation runs at 100% real-time speed (1.023 MHz effective) on any modern Windows PC without frame drops during normal operation
- **SC-007**: All existing 577+ Casso65 unit tests continue to pass after adding the new project
- **SC-008**: A developer can add support for a new machine type by creating a JSON config file and writing device component classes, without modifying the emulator shell, MemoryBus, or any existing machine configs
- **SC-009**: Apple IIe emulation supports 80-column text mode and double hi-res graphics, running software that requires these features
- **SC-010**: The emulator exits cleanly when encountering configuration errors, providing error messages that identify the specific problem (missing ROM, unknown device, invalid JSON) and suggest corrective action

## Assumptions

- Users provide their own Apple II ROM images (these are copyrighted and will not be distributed with the project); ROM files are gitignored
- The emulator targets Windows 10/11 on x64 and ARM64 architectures, consistent with existing Casso65 platform support
- The Win32 GDI rendering approach is sufficient for the 1 MHz Apple II's display refresh requirements; no GPU acceleration is needed
- The existing Casso65Core 6502 CPU emulation is cycle-accurate enough to run Apple II software correctly; if timing adjustments are needed they will be addressed as bugs, not as spec changes
- Disk II emulation supports the standard 140KB .dsk format (DOS-order, 16 sectors × 35 tracks); other formats (e.g., .nib, .woz) are out of scope for the initial implementation
- Audio output uses the Windows waveOut or similar built-in audio API; low-latency audio is desirable but not a hard requirement for initial delivery
- The Apple II+ and original Apple II differ only in ROM content (Applesoft vs. Integer BASIC); both use the same machine config structure with different ROM file references
- The focus is on accurate functional emulation, not cycle-exact hardware reproduction; minor timing differences that don't affect software compatibility are acceptable

## Machine Configuration File Schema

Machine configs are JSON files stored in `Casso65Emu/machines/`. Each file defines the complete hardware configuration for one computer model. The emulator loads the config at startup based on the `--machine` argument.

### Config Fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Human-readable machine name (e.g., "Apple II+") |
| `cpu` | string | CPU variant: `"6502"`, `"65c02"`, `"nes2a03"` |
| `clockSpeed` | int | Clock speed in Hz (e.g., 1023000 for Apple II) |
| `memory` | array | RAM and ROM region definitions |
| `memory[].type` | string | `"ram"` or `"rom"` |
| `memory[].start` | string | Hex start address (e.g., `"0x0000"`) |
| `memory[].end` | string | Hex end address (e.g., `"0xBFFF"`) |
| `memory[].file` | string | ROM filename (rom type only), resolved relative to roms directory |
| `devices` | array | Named device instances wired to the address bus |
| `devices[].type` | string | Component registry name (e.g., `"apple2-keyboard"`) |
| `devices[].address` | string | Single address for point-mapped devices |
| `devices[].start` / `devices[].end` | string | Address range for range-mapped devices |
| `devices[].slot` | int | Slot number for slot-based devices (Apple II slots 1–7) |
| `video` | object | Video output configuration |
| `video.modes` | array | List of video mode component names (e.g., `["apple2-text40", "apple2-lores", "apple2-hires"]`) |
| `video.width` | int | Display framebuffer width in pixels |
| `video.height` | int | Display framebuffer height in pixels |
| `keyboard` | object | Keyboard input configuration |
| `keyboard.type` | string | Keyboard component name (e.g., `"apple2-uppercase"`, `"apple2e-full"`) |

### Example: Apple II+ Config

```json
{
    "name": "Apple II+",
    "cpu": "6502",
    "clockSpeed": 1023000,
    "memory": [
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF" },
        { "type": "rom",  "file": "apple2plus.rom", "start": "0xD000", "end": "0xFFFF" }
    ],
    "devices": [
        { "type": "apple2-keyboard",      "address": "0xC000" },
        { "type": "apple2-speaker",       "address": "0xC030" },
        { "type": "apple2-softswitches",  "start": "0xC050", "end": "0xC05F" },
        { "type": "language-card",        "start": "0xC080", "end": "0xC08F" },
        { "type": "disk-ii",             "slot": 6 }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-lores", "apple2-hires"],
        "width": 560,
        "height": 384
    },
    "keyboard": {
        "type": "apple2-uppercase"
    }
}
```

### Example: Apple IIe Config (differences from II+)

```json
{
    "name": "Apple IIe",
    "cpu": "65c02",
    "clockSpeed": 1023000,
    "memory": [
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF" },
        { "type": "ram",  "start": "0x0000", "end": "0xBFFF", "bank": "aux" },
        { "type": "rom",  "file": "apple2e.rom",      "start": "0xC100", "end": "0xFFFF" },
        { "type": "rom",  "file": "apple2e-char.rom",  "start": "0x0000", "end": "0x07FF", "target": "chargen" }
    ],
    "devices": [
        { "type": "apple2e-keyboard",     "address": "0xC000" },
        { "type": "apple2-speaker",       "address": "0xC030" },
        { "type": "apple2e-softswitches", "start": "0xC050", "end": "0xC05F" },
        { "type": "aux-ram-card",         "start": "0xC003", "end": "0xC005" },
        { "type": "language-card",        "start": "0xC080", "end": "0xC08F" },
        { "type": "disk-ii",             "slot": 6 }
    ],
    "video": {
        "modes": ["apple2-text40", "apple2-text80", "apple2-lores", "apple2-hires", "apple2-doublehires"],
        "width": 560,
        "height": 384
    },
    "keyboard": {
        "type": "apple2e-full"
    }
}
```

## Component Registry Architecture

The component registry maps string type names from the config to C++ factory functions. This is the boundary between data-driven configuration and code-driven behavior.

### Components that require code (behavioral logic)

These components contain state machines, hardware protocols, or rendering algorithms that cannot be expressed as data.

| Config type string | C++ class | Behavior requiring code |
|--------------------|-----------|------------------------|
| `"apple2-keyboard"` | `AppleKeyboard` | Host-to-Apple key mapping, ASCII encoding, strobe latch at $C000/$C010 |
| `"apple2e-keyboard"` | `AppleIIeKeyboard` | Adds lowercase, modifier keys (Open/Closed Apple), auto-repeat |
| `"apple2-speaker"` | `AppleSpeaker` | Toggle state on $C030 read, audio sample generation |
| `"apple2-softswitches"` | `AppleSoftSwitchBank` | Video mode toggles ($C050–$C057), mixed mode, page select — state machine driving video mode selection |
| `"apple2e-softswitches"` | `AppleIIeSoftSwitchBank` | Extends with 80-col, aux RAM bank select, IIe-specific switches |
| `"language-card"` | `LanguageCard` | Bank-switching state machine: RAM/ROM select, write-enable sequencing at $C080–$C08F |
| `"aux-ram-card"` | `AuxRamCard` | IIe auxiliary 64KB RAM bank switching at $C003/$C005 |
| `"disk-ii"` | `DiskIIController` | Stepper motor state machine, read/write head, nibble encoding/decoding, .dsk sector translation |
| `"apple2-text40"` | `AppleTextMode` | Interleaved row address decoding, character ROM lookup, cursor blink |
| `"apple2-text80"` | `Apple80ColTextMode` | Interleaved main/aux memory for 80-column display |
| `"apple2-lores"` | `AppleLoResMode` | Nybble-to-color decoding, 16-color palette |
| `"apple2-hires"` | `AppleHiResMode` | 7-pixels-per-byte decoding, palette bit, NTSC color artifacting algorithm |
| `"apple2-doublehires"` | `AppleDoubleHiResMode` | 560×192 from interleaved main/aux memory, 16-color palette |

### Components that are purely data-driven

These are generic and fully configured by the JSON — no device-specific C++ class needed.

| Config type string | C++ class | Configuration |
|--------------------|-----------|---------------|
| `"ram"` | `RamDevice` | Start/end address, optional bank name |
| `"rom"` | `RomDevice` | Start/end address, file path |

### Registry initialization

At program startup, all device factories are registered:

```cpp
void RegisterBuiltinDevices (ComponentRegistry & registry)
{
    registry.Register ("ram",                   RamDevice::Create);
    registry.Register ("rom",                   RomDevice::Create);
    registry.Register ("apple2-keyboard",       AppleKeyboard::Create);
    registry.Register ("apple2e-keyboard",      AppleIIeKeyboard::Create);
    registry.Register ("apple2-speaker",        AppleSpeaker::Create);
    registry.Register ("apple2-softswitches",   AppleSoftSwitchBank::Create);
    registry.Register ("apple2e-softswitches",  AppleIIeSoftSwitchBank::Create);
    registry.Register ("language-card",         LanguageCard::Create);
    registry.Register ("aux-ram-card",          AuxRamCard::Create);
    registry.Register ("disk-ii",              DiskIIController::Create);
}
```

### Adding a future machine (e.g., Commodore 64)

1. Write new device classes: `VicII`, `Sid`, `Cia`, `C64Keyboard`
2. Register them in `RegisterBuiltinDevices`
3. Create `machines/c64.json` with the C64 memory map, ROM paths, and device wiring
4. No changes to the emulator shell, MemoryBus, or existing machine configs

### ROM sourcing

ROM images are copyrighted and not distributed with the project. The `roms/` directory is gitignored. A helper script `scripts/FetchRoms.ps1` can download ROMs from the AppleWin project's repository (https://github.com/AppleWin/AppleWin) for convenience, following the same pattern as `scripts/RunDormannTest.ps1`.
