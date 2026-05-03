# Changelog

All notable changes to Casso are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/).
Versioned entries use `MAJOR.MINOR.BUILD` from [Version.h](CassoCore/Version.h).
Entries before versioning was introduced use dates only.

## [1.0.244] — 2026-05-03

### Added
- **Apple II platform emulator (Casso.exe)** — GUI-based Apple II, II+, and IIe emulator
  with D3D11 rendering, WASAPI audio, data-driven JSON machine configs, and keyboard input
- **CPU thread architecture** — dedicated CPU thread for 6502 execution and audio,
  UI thread for Win32 messages and D3D Present with vsync
- **Status bar** — shows CPU type, clock speed (MHz), machine name, and device count;
  clicking devices shows a popup listing all bus-mapped devices with address ranges
- **Edit menu** — Copy Text (reads 40×24 text screen as ASCII), Copy Screenshot
  (framebuffer as DIB bitmap), Paste (Ctrl+V feeds clipboard into keyboard)
- **Window base class** — ported from Mandelbrot repo, virtual On* message handlers;
  EmulatorShell and DebugConsole derive from it
- **ComPtr\<T\>** — Microsoft::WRL::ComPtr alias replaces all raw COM pointer management
  in D3DRenderer and WasapiAudio; eliminates manual Release() chains
- **Table-driven menus** — MenuSystem uses static MenuItem arrays and BuildPopupMenu loop
- **Apple II key constants** — kAppleKeyLeft/Right/Up/Down/Escape/Delete in AppleKeyboard.h
- **JsonValue typed accessors** — GetString/GetInt/GetUint32/GetBool/GetObject/GetArray
  with HRESULT + out param; specific error codes (JSON_E_KEY_MISSING, JSON_E_TYPE_MISMATCH)
- **EHM unified architecture** — single \_\_EHM\_Base macro; F suffix for failure actions
  (CHRF, CBRF, CWRF); N suffix implemented as F + EhmNotifyUser; legacy \_SetError removed
- **Cycle-accurate instruction timing** — baseCycles in Microcode with runtime page-cross
  and branch-taken penalties
- **1ms execution slicing** — CPU runs in ~1023-cycle slices with per-slice audio submission,
  inspired by AppleWin's architecture
- **Pending audio buffer** — decouples PCM generation from WASAPI drain to prevent stutter
- **PathResolver with std::filesystem::path** — replaces manual string concatenation and
  WideToNarrow/NarrowToWide conversion functions
- **DPI-scaled debug console font** — uses GetDpiForWindow + MulDiv

### Changed
- **Project rename** — Casso65Core → CassoCore, Casso65EmuCore → CassoEmuCore,
  Casso65Emu → Casso, Casso65 → CassoCli; repo renamed to relmer/Casso
- **Exact NTSC timing** — CPU clock 1,022,727 Hz (was 1,023,000), cycles/frame 17,030
  (was 17,050); derived from 14.31818 MHz crystal
- **Speaker amplitude** — ±0.25f (was ±1.0f) to match reference audio levels
- **WASAPI buffer** — 100ms (was 33ms) for jitter headroom
- **D3D vsync** — Present(1) on UI thread, Present(0) was double-gating with frame timer
- **using namespace std** + **namespace fs** in both Emu Pch.h files
- **In-class member initialization** preferred over constructor initializer lists
- **Casso65Emu flattened** — removed Audio/, Shell/, Resources/, shaders/ subdirectories
- **machines/ → Machines/, roms/ → ROMs/** — directory casing standardized

### Fixed
- **Mixed-mode text flicker** — framebuffer race condition; CPU thread now copies completed
  framebuffer to UI buffer under mutex
- **Hi-res NTSC colors** — two-pass renderer correctly handles cross-byte-boundary adjacent
  pixels; HCOLOR=3 renders as solid white
- **Power cycle** — now clears RAM ($0000-$BFFF) for cold boot with APPLE ][ banner
- **Paste drops characters** — DrainPasteBuffer now checks keyboard strobe before feeding
  next character
- **Duplicate AddDevice** — every device was registered twice on the memory bus
- **Bus overlap detection** — Validate() uses CBRN with specific conflicting address ranges
- **Title bar garbage** — em-dash encoded as UTF-8 in source, replaced with \\u2014 escape
- **Debug console newlines** — bare LF converted to CRLF for Win32 EDIT control
- **Audio buzz during boot** — capped submission to one frame, pre-filled silence
- **Green screen** — CPU opcode fetch now uses virtual ReadByte through MemoryBus
- **Black screen** — D3D11 shaders implemented via runtime D3DCompile
- **ParseHexAddress** — overflow and invalid-char validation added

## [0.9.32] — 2026-04-28

### Added
- Tom Harte SingleStepTests — per-opcode validation against 151 legal-opcode test sets (10,000 vectors each)
- `run` subcommand — load and execute a binary or assembly source from the CLI
- **Full AS65-compatible assembler** — from-scratch reimplementation of Frank A. Kingswood's AS65
  - All 56 mnemonics, all 14 addressing modes
  - Two-pass assembly with forward-reference resolution
  - Full expression evaluator: `+ - * / % & | ^ ~ << >>`, `<`/`>` byte selectors, current-PC `*`
  - Constants: `equ`, `=`, `set` with forward-reference chains
  - Conditional assembly: `if`/`ifdef`/`ifndef`/`else`/`endif`
  - Macros: `name macro`…`endm`, positional `\1`–`\9` and named parameters, `local`, `exitm`, `\?` unique suffix
  - Directives: `.org`, `.byte`, `.word`, `.text`, `.ds`, `.dd`, `.align`, `.end`, `.error`, `.include`
  - Struct definitions with `struct`/`end struct` and dot-qualified member access
  - Character map (`.cmap`) for custom character encodings
  - Three-segment model: `code`/`data`/`bss`
  - Binary file includes: `.bin`, `.s19`, `.hex` (incbin-style)
  - Backslash line continuation in macros
  - Colon-less labels (AS65 compatibility)
  - Listing output: `-l [file]`, `-c` cycle counts, `-m` macro expansion, `-t` symbol table
  - Output formats: flat binary (default), `-s` Motorola S-record, `-s2` Intel HEX
  - Warning control: `--warn`, `--no-warn`, `--fatal-warnings`
  - Flag concatenation: `-tlfile` ≡ `-t -l file` (AS65 style)
  - 10-test conformance suite verifying AS65 parity
- Dormann and Harte regression test suites with on-demand runner scripts

### Fixed
- BCD flag behavior (N/V/Z flags match real 6502 hardware)
- JMP indirect page-boundary wrap bug
- JSR operand read overlapping stack push
- DEY/PLA opcode table swap
- STY missing source register in absolute mode
- Addressing mode wrapping for zero-page indexed modes
- Assembler: `equ` forward-reference chain resolution, `ifdef`/`ifndef`, `-s2` Intel HEX output, listing column layout, `.org` gap fill
- LDX/INC/DEC addressing mode table entries
- STY missing Absolute addressing mode (#37)

## 2026-04-25

### Changed
- Project renamed from **My6502** to **Casso**

## 2026-04-24

### Added
- **Assembler v1** (spec 001) — basic two-pass assembler with labels, branches, directives, expressions, listing output, and CLI subcommands
- `LoadBinary()` — load pre-assembled binaries into CPU memory
- CI pipeline with GitHub Actions (x64 + ARM64, Debug + Release)

### Fixed
- `ShiftLeft`/`ShiftRight` dispatch (was calling `RotateLeft`/`RotateRight`)
- `BIT` instruction V/N flags (were read from AND result instead of operand)
- `Compare` carry flag for boundary values
- `PushWord`/`PopWord` read/write outside stack page

## 2026-04-23

### Added
- Extracted `CassoCore` static library from monolithic project
- `UnitTest` project with 66 initial tests (Microsoft Native CppUnitTest)
- Build/test automation scripts and VS Code tasks

## 2024-12-15

### Added
- Stack and memory helpers, rewritten addressing mode resolution
- `BRK` (software interrupt) implementation

### Fixed
- `LDX`, `DEX`, `BMI`, `BPL`, `INX` behavior corrections

## 2024-12-08

### Added
- Flag manipulation (`CLC`, `SEC`, `CLI`, `SEI`, `CLV`, `CLD`, `SED`), register transfers (`TAX`, `TXA`, `TAY`, `TYA`, `TXS`, `TSX`), `NOP`

## 2024-11-24 — 2024-11-30

### Added
- Initial 6502 emulator: fetch-decode-execute cycle, all 56 standard mnemonics
- **Group 01**: `ORA`, `AND`, `EOR`, `ADC`, `STA`, `LDA`, `CMP`, `SBC` — all 8 addressing modes
- **Group 00**: `BIT`, `JMP`, `STY`, `LDY`, `CPY`, `CPX`
- **Group 10**: `ASL`, `LSR`, `ROL`, `ROR`, `STX`, `LDX`, `DEC`, `INC`
- All 14 addressing modes (immediate, zero page, ZP,X, ZP,Y, absolute, abs,X, abs,Y, (ZP,X), (ZP),Y, indirect, relative, accumulator, implied, jump absolute)
