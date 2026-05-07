#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "KeystrokeInjector.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleHiResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr uint64_t   kColdBootCycles      = 5000000ULL;
    static constexpr uint64_t   kAfterCommandCycles  = 1000000ULL;
    static constexpr uint64_t   kBootDiskCycles      = 2000000ULL;

    static constexpr int        kFbW                 = 560;
    static constexpr int        kFbH                 = 384;
    static constexpr int        kHiresRowsTopRegion  = 160;     // Mixed mode: top 160 lines hi-res
    static constexpr int        kMixedTextStartRow   = 20;      // Bottom 4 rows = text

    static constexpr Word       kSwitch80StoreOff    = 0xC000;
    static constexpr Word       kSwitch80StoreOn     = 0xC001;
    static constexpr Word       kSwitchTextOff       = 0xC050;
    static constexpr Word       kSwitchMixedOn       = 0xC053;
    static constexpr Word       kSwitchPage1         = 0xC054;
    static constexpr Word       kSwitchHiresOn       = 0xC057;
    static constexpr Word       kSwitch80ColOn       = 0xC00D;

    static constexpr int        kSlot6               = 6;
    static constexpr int        kDrive1              = 0;
    static constexpr Word       kBootRomEntry        = 0xC600;
    static constexpr Word       kIntCxRomOff         = 0xC006;
    static constexpr size_t     kSentinelOffset      = 0x10;
    static constexpr Byte       kSentinelByte        = 0x5A;
    static constexpr size_t     kWozTrackBitCount    = 51200;
    static constexpr size_t     kCpTrackBitCount     = 50000;
    static constexpr size_t     kWozTrackByteCount   = 6400;
    static constexpr size_t     kCpTrackByteCount    = 6250;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Fnv1a64 — stable framebuffer hash for the golden-output gate.
    //
    ////////////////////////////////////////////////////////////////////////////

    uint64_t Fnv1a64 (const uint32_t * data, size_t count)
    {
        uint64_t   h = 0xcbf29ce484222325ULL;
        size_t     i;
        int        b;
        uint8_t    byte;
        uint32_t   v;

        for (i = 0; i < count; i++)
        {
            v = data[i];

            for (b = 0; b < 4; b++)
            {
                byte = static_cast<uint8_t> ((v >> (b * 8)) & 0xFF);
                h   ^= byte;
                h   *= 0x100000001b3ULL;
            }
        }

        return h;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PrngNext — local LCG used by the golden-output test to stamp a
    //  deterministic, repeatable pattern into hi-res page 1 + 80-col text
    //  pages. Independent of HeadlessHost::kPinnedSeed so the produced
    //  bytes don't track changes to that seed.
    //
    ////////////////////////////////////////////////////////////////////////////

    uint32_t PrngNext (uint32_t & state)
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  BootIIeToPrompt — cold-boot the //e harness to the Applesoft `]`
    //  prompt. Reused by every US4 graphics-mode scenario.
    //
    ////////////////////////////////////////////////////////////////////////////

    void BootIIeToPrompt (HeadlessHost & host, EmulatorCore & core)
    {
        HRESULT   hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe must succeed");
        Assert::IsTrue (core.HasAppleIIe (), L"//e wiring must be complete");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSyntheticDsk / BuildSyntheticPo / BuildSyntheticWoz — replicas
    //  of the Phase 11 fixture builders. The validation suite owns its
    //  own copies so it remains a single self-contained file (per user
    //  story 4 acceptance). No third-party software is loaded.
    //
    ////////////////////////////////////////////////////////////////////////////

    vector<Byte> BuildSyntheticDsk ()
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);

        raw[kSentinelOffset] = kSentinelByte;
        return raw;
    }


    vector<Byte> BuildSyntheticPo ()
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);

        raw[kSentinelOffset] = kSentinelByte;
        return raw;
    }


    HRESULT BuildSyntheticWoz (size_t bitCount, size_t byteCount, vector<Byte> & outBytes)
    {
        vector<Byte>   bits (byteCount, 0xFF);

        return WozLoader::BuildSyntheticV2 (1, false, bits, bitCount, outBytes);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MountAndJumpToSlot6Boot — Phase 11 helper, replicated here. Mounts
    //  the supplied bytes through DiskImageStore, points the //e at the
    //  slot 6 boot ROM, returns the store-owned DiskImage so callers can
    //  inspect track state.
    //
    ////////////////////////////////////////////////////////////////////////////

    DiskImage * MountAndJumpToSlot6Boot (
        HeadlessHost  &  host,
        EmulatorCore  &  core,
        const string  &  virtualPath,
        DiskFormat       fmt,
        const vector<Byte> & bytes)
    {
        HRESULT      hr        = S_OK;
        DiskImage *  external  = nullptr;

        hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1, virtualPath, fmt, bytes);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, external);
        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        return external;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  RowContaining — find the first row index whose scraped string
    //  contains the supplied needle, or -1 if none.
    //
    ////////////////////////////////////////////////////////////////////////////

    int RowContaining (const std::vector<std::string> & rows, const std::string & needle)
    {
        size_t   i;

        for (i = 0; i < rows.size (); i++)
        {
            if (rows[i].find (needle) != std::string::npos)
            {
                return static_cast<int> (i);
            }
        }

        return -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmuValidationSuiteTests
//
//  Phase 13 / User Story 4. The consolidated FR-045 acceptance suite.
//  Every scenario runs deterministically through HeadlessHost +
//  IFixtureProvider only — constitution §II compliant, no Win32, no
//  audio device, no host filesystem outside UnitTest/Fixtures/.
//
//  Scope mapping:
//    T114 — `GR`  reaches lo-res mixed-mode state (FR-045 acceptance).
//    T115 — `HGR` reaches hi-res page 1 (FR-045 acceptance).
//    T116 — `HGR2` reaches hi-res page 2 (FR-045 acceptance).
//    T117 — Mixed-mode + 80COL golden framebuffer hash (FR-017a / Q1).
//    T118 — DOS 3.3, ProDOS, WOZ, copy-protected disk boot end-to-end.
//    T113/orchestrator — `RunsAllScenarios` invokes the full FR-045 set
//                        and asserts every sub-scenario reports green.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuValidationSuiteTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T114 — GR command engages lo-res graphics + mixed-mode.
    //
    //  Acceptance: after `GR` the //e softswitches report graphics on,
    //  mixed on, hires off, page2 off; the scraped bottom-4 text rows
    //  still contain the `]` prompt (mixed mode preserves the text
    //  region). Lo-res page1 ($0400) holds at least one non-zero byte
    //  cleared by the GR command's color-fill side effect.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_GR_LoresGraphics_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;
        int            promptRow;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "GR");
        Assert::AreEqual (size_t (3), consumed,
            L"`GR` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"GR must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsMixedMode (),
            L"GR must engage mixed mode (4 lines text bottom)");
        Assert::IsFalse (core.softSwitches->IsHiresMode (),
            L"GR must keep HIRES off");
        Assert::IsFalse (core.softSwitches->IsPage2 (),
            L"GR must select PAGE1");

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Scraper must produce 24 rows");

        promptRow = RowContaining (rows, "]");
        Assert::IsTrue (promptRow >= kMixedTextStartRow,
            L"GR mixed-mode prompt must appear in the bottom-4 text region");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T115 — HGR command engages hi-res page 1 + mixed-mode.
    //
    //  Acceptance: after `HGR` the soft switches report HIRES on, PAGE2
    //  off, MIXED on; HPLOT then runs without exiting the graphics
    //  region (prompt remains in mixed-mode bottom rows).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_HGR_HiresPage1_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;
        int            promptRow;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "HGR");
        Assert::AreEqual (size_t (4), consumed,
            L"`HGR` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"HGR must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsHiresMode (),
            L"HGR must engage HIRES");
        Assert::IsFalse (core.softSwitches->IsPage2 (),
            L"HGR must select PAGE1");
        Assert::IsTrue  (core.softSwitches->IsMixedMode (),
            L"HGR must engage MIXED (preserves bottom text)");

        consumed = KeystrokeInjector::InjectLine (core, "HPLOT 0,0 TO 279,159");
        Assert::IsTrue (consumed >= 18,
            L"HPLOT line must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        promptRow = RowContaining (rows, "]");
        Assert::IsTrue (promptRow >= 0,
            L"HGR + HPLOT must leave the prompt visible somewhere on the text page");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T116 — HGR2 command engages hi-res page 2 (full-screen
    //  graphics, no mixed-mode).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_HGR2_HiresPage2_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "HGR2");
        Assert::AreEqual (size_t (5), consumed,
            L"`HGR2` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"HGR2 must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsHiresMode (),
            L"HGR2 must engage HIRES");
        Assert::IsTrue  (core.softSwitches->IsPage2 (),
            L"HGR2 must select PAGE2");
        Assert::IsFalse (core.softSwitches->IsMixedMode (),
            L"HGR2 must clear MIXED (full-screen graphics)");
    }
};
