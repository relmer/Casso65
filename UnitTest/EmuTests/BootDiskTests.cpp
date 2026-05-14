#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>

#include "Cpu.h"
#include "Assembler.h"
#include "AssemblerTypes.h"

#include "HeadlessHost.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/DiskIIController.h"
#include "Devices/AppleIIeSoftSwitchBank.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskTests
//
//  End-to-end gate for the full Apple //e disk-boot pipeline using a
//  100% in-house demo disk -- no third-party software, no copyrighted
//  content. Source for the demo lives at Apple2/Demos/casso-rocks.a65;
//  the framebuffer payload at Apple2/Demos/cassowary.hgr is generated
//  by scripts/HgrPreprocess.py from one of the cassowary photos under
//  Assets/. This test assembles the .a65 via Casso's own AS65-compatible
//  assembler, stitches the HGR payload behind the boot sector at file
//  offset $1000 (track 1 sector 0), mounts the resulting .dsk through
//  DiskImageStore, lets the //e boot ROM pick it up, and verifies the
//  demo:
//
//    - reads the 8 KB framebuffer off tracks 1+2 (32 sectors) into
//      $2000-$3FFF using a from-scratch 6502 RWTS,
//    - flips the soft switches into HGR1 mode (TEXT off, MIXED off,
//      PAGE2 off, HIRES on),
//    - leaves the cassowary on screen.
//
//  Exercises: HgrPreprocess.py output -> Assembler -> NibblizationLayer
//  -> DiskImageStore -> DiskIIController -> DiskIINibbleEngine ->
//  Disk2.rom slot 6 boot -> 6502 CPU executing our RWTS -> MMU HGR
//  page-1 writes -> AppleIIeSoftSwitchBank graphics-mode latching.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int     kMaxAncestorWalk     = 10;
    static constexpr size_t  kHgrPayloadSize      = 8192;
    static constexpr size_t  kLoresPayloadSize    = 1024;
    static constexpr size_t  kSectorByteSize      = 256;
    static constexpr int     kSectorsPerTrack     = 16;
    static constexpr Word    kHgrBase             = 0x2000;
    static constexpr Word    kBootEntry           = 0xC600;
    static constexpr Word    kDemoEntry           = 0x0801;
    static constexpr Word    kStage2Entry         = 0x1000;
    static constexpr uint64_t  kDemoCycleBudget   = 60'000'000ULL;


    fs::path FindRepoFile (const std::string & relPath)
    {
        std::error_code ec;
        fs::path        cursor = fs::current_path (ec);
        if (ec) return fs::path ();

        for (int i = 0; i < kMaxAncestorWalk; i++)
        {
            fs::path candidate = cursor / relPath;
            if (fs::exists (candidate, ec)) return candidate;
            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }
            cursor = cursor.parent_path ();
        }
        return fs::path ();
    }


    std::string ReadFileText (const fs::path & path)
    {
        std::ifstream f (path);
        if (!f) return std::string ();
        return std::string ((std::istreambuf_iterator<char> (f)),
                            std::istreambuf_iterator<char> ());
    }


    std::vector<Byte> ReadFileBytes (const fs::path & path)
    {
        std::ifstream f (path, std::ios::binary);
        if (!f) return std::vector<Byte> ();
        return std::vector<Byte> ((std::istreambuf_iterator<char> (f)),
                                  std::istreambuf_iterator<char> ());
    }
}


TEST_CLASS (BootDiskTests)
{
public:

    TEST_METHOD (CassoRocks_DemoDisk_DisplaysHgrCassowary)
    {
        fs::path  src           = FindRepoFile ("Apple2/Demos/casso-rocks.a65");
        fs::path  stage2Src     = FindRepoFile ("Apple2/Demos/casso-rocks-stage2.a65");
        fs::path  hgrPath       = FindRepoFile ("Apple2/Demos/cassowary.hgr");
        fs::path  bandsPath     = FindRepoFile ("Apple2/Demos/test-bands.hgr");
        fs::path  loresPath     = FindRepoFile ("Apple2/Demos/lores-bars.lores");

        if (src.empty () || stage2Src.empty () ||
            hgrPath.empty () || bandsPath.empty () ||
            loresPath.empty ())
        {
            Logger::WriteMessage ("SKIPPED: one or more demo-disk source "
                                  "files (casso-rocks*.a65, cassowary.hgr, "
                                  "test-bands.hgr, lores-bars.lores) not "
                                  "found in this checkout.\n");
            return;
        }

        std::string source       = ReadFileText (src);
        std::string stage2Source = ReadFileText (stage2Src);
        Assert::IsFalse (source.empty (), L"casso-rocks.a65 must not be empty");
        Assert::IsFalse (stage2Source.empty (),
            L"casso-rocks-stage2.a65 must not be empty");

        std::vector<Byte>  hgrPayload   = ReadFileBytes (hgrPath);
        std::vector<Byte>  bandsPayload = ReadFileBytes (bandsPath);
        std::vector<Byte>  loresPayload = ReadFileBytes (loresPath);
        Assert::AreEqual (kHgrPayloadSize, hgrPayload.size (),
            L"cassowary.hgr must be exactly 8192 bytes");
        Assert::AreEqual (kHgrPayloadSize, bandsPayload.size (),
            L"test-bands.hgr must be exactly 8192 bytes");
        Assert::AreEqual (kLoresPayloadSize, loresPayload.size (),
            L"lores-bars.lores must be exactly 1024 bytes");

        Cpu             cpu;
        Assembler       assembler (cpu.GetInstructionSet ());

        AssemblyResult  asmResult = assembler.Assemble (source);
        if (!asmResult.success)
        {
            wchar_t  msg[256] = {};
            const char *  firstError = asmResult.errors.empty ()
                ? "(no error message)"
                : asmResult.errors[0].message.c_str ();
            swprintf_s (msg, L"casso-rocks.a65 must assemble cleanly. First "
                             L"error: %hs", firstError);
            Assert::Fail (msg);
        }

        AssemblyResult  stage2Result = assembler.Assemble (stage2Source);
        if (!stage2Result.success)
        {
            wchar_t  msg[256] = {};
            const char *  firstError = stage2Result.errors.empty ()
                ? "(no error message)"
                : stage2Result.errors[0].message.c_str ();
            swprintf_s (msg, L"casso-rocks-stage2.a65 must assemble cleanly. "
                             L"First error: %hs", firstError);
            Assert::Fail (msg);
        }

        Assert::AreEqual (Word (kDemoEntry), asmResult.startAddress,
            L"Stage 1 must be assembled with .org $0801 (boot ROM JMP target)");
        Assert::IsTrue (asmResult.bytes.size () > 0 &&
                        asmResult.bytes.size () <= kSectorByteSize - 1,
            L"Stage 1 code must fit in the remainder of sector 0 ($0801-$08FF)");
        Assert::AreEqual (Word (kStage2Entry), stage2Result.startAddress,
            L"Stage 2 must be assembled with .org $0A00");
        Assert::IsTrue (stage2Result.bytes.size () > 0 &&
                        stage2Result.bytes.size () <= kSectorByteSize,
            L"Stage 2 code must fit in a single 256-byte sector");

        // Build a 143360-byte raw .dsk image:
        //   - File offset 1..N (track 0 sector 0 minus the first byte):
        //     stage 1 boot code.
        //   - Tracks 1+2 (file offsets $1000..$3000): 8 KB cassowary
        //     framebuffer.
        //   - Track 3 logical sector 0 (file offset $3000): stage 2
        //     code (lands at $1000).
        //     Track 3 logical sectors 1..4 (file offsets via LtoP):
        //     1 KB LoRes test pattern (lands at $1100-$14FF, then
        //     gets memcpy'd into text page 1 when stage 2 cycles
        //     into LoRes mode).
        //   - Tracks 4+5 (file offsets $4000..$6000): 8 KB HGR test
        //     bands (mode 1 of the cycle).
        //   - Tracks 6+7 (file offsets $6000..$8000): 8 KB DHGR aux
        //     pattern (loaded into aux $2000-$3FFF via RAMWRT during
        //     stage 2 init).
        //   - Tracks 8+9 (file offsets $8000..$A000): 8 KB DHGR main
        //     pattern (loaded into main $2000-$3FFF when stage 2
        //     cycles into DHGR mode -- overwrites cassowary, which
        //     is one-way OK for this demo).
        //   The HGR payloads use the DOS 3.3 logical-to-physical
        //   interleave so that when our RWTS reads logical sector S
        //   of track T it gets exactly payload[((T-startTrack)*16+S)*256..].
        static constexpr int  kDsk_LtoP[16] =
        {
            0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
        };

        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);

        for (size_t i = 0; i < asmResult.bytes.size (); i++)
        {
            raw[1 + i] = asmResult.bytes[i];
        }

        // Track 3 logical sector 0 -> stage 2 code at $1000.
        // Track 3 logical sectors 1-4 -> LoRes pattern at $1100-$14FF.
        // Use the LtoP mapping so the on-disk physical layout the
        // nibblizer produces matches what stage 1's RWTS expects to
        // read out by logical address.
        auto StampTrack3Sector = [&] (int logicalSector,
                                      const Byte * data, size_t len)
        {
            size_t  fileOffset = static_cast<size_t> (
                3 * kSectorsPerTrack + kDsk_LtoP[logicalSector])
                * kSectorByteSize;
            for (size_t i = 0; i < len; i++)
            {
                raw[fileOffset + i] = data[i];
            }
        };

        StampTrack3Sector (0, stage2Result.bytes.data (),
                              stage2Result.bytes.size ());
        for (int sector = 0; sector < 4; sector++)
        {
            StampTrack3Sector (1 + sector,
                               loresPayload.data () + sector * kSectorByteSize,
                               kSectorByteSize);
        }

        // Stitch a payload across 2 tracks starting at startTrack.
        auto StitchPayload = [&raw] (int startTrack,
                                     const std::vector<Byte> & payload)
        {
            for (int trackOffset = 0; trackOffset < 2; trackOffset++)
            {
                int  track = startTrack + trackOffset;

                for (int sector = 0; sector < kSectorsPerTrack; sector++)
                {
                    size_t  fileOffset =
                        static_cast<size_t> (track * kSectorsPerTrack + kDsk_LtoP[sector])
                        * kSectorByteSize;
                    size_t  payloadOffset =
                        static_cast<size_t> (trackOffset * kSectorsPerTrack + sector)
                        * kSectorByteSize;

                    for (size_t i = 0; i < kSectorByteSize; i++)
                    {
                        raw[fileOffset + i] = payload[payloadOffset + i];
                    }
                }
            }
        };

        StitchPayload (1, hgrPayload);       // tracks 1+2 -> cassowary @ $2000
        StitchPayload (4, bandsPayload);     // tracks 4+5 -> HGR bands @ $4000

        HeadlessHost  host;
        EmulatorCore  core;

        HRESULT  hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (6, 0, "casso-rocks.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        DiskImage *  img = core.diskStore->GetImage (6, 0);
        Assert::IsNotNull (img);
        core.diskController->SetExternalDisk (0, img);

        core.bus->WriteByte (0xC006, 0);  // INTCXROM=0

        core.cpu->SetPC (kBootEntry);

        core.RunCycles (kDemoCycleBudget);

        // ----- Verify HGR1 soft-switch state -----
        AppleIIeSoftSwitchBank *  ss = core.softSwitches.get ();

        Assert::IsNotNull (ss, L"AppleIIeSoftSwitchBank must be present");
        Assert::IsTrue (ss->IsGraphicsMode (),
            L"Demo must leave the //e in graphics mode (TEXT off)");
        Assert::IsFalse (ss->IsMixedMode (),
            L"Demo must leave MIXED off (full-screen HGR)");
        Assert::IsFalse (ss->IsPage2 (),
            L"Demo must leave PAGE2 off (HGR page 1 visible) before any keystroke");
        Assert::IsTrue (ss->IsHiresMode (),
            L"Demo must leave HIRES on");

        // ----- Verify HGR framebuffer contents (page 1 + page 2) -----
        // The 6502 RWTS reads 256 bytes per sector and stores them via
        // ($26),Y. Compare every byte against the source payload so any
        // single-bit error in seek / address-mark scan / 6+2 expansion
        // surfaces immediately.
        auto VerifyPage = [&] (Word baseAddr,
                               const std::vector<Byte> & expected,
                               const wchar_t * label)
        {
            size_t  mismatchCount      = 0;
            size_t  firstMismatch      = 0;
            Byte    expectedAtMismatch = 0;
            Byte    actualAtMismatch   = 0;

            for (size_t i = 0; i < kHgrPayloadSize; i++)
            {
                Byte  e = expected[i];
                Byte  actual = core.bus->ReadByte (
                    static_cast<Word> (baseAddr + i));

                if (actual != e)
                {
                    if (mismatchCount == 0)
                    {
                        firstMismatch       = i;
                        expectedAtMismatch  = e;
                        actualAtMismatch    = actual;
                    }
                    mismatchCount++;
                }
            }

            if (mismatchCount != 0)
            {
                wchar_t  msg[256] = {};
                swprintf_s (msg, L"%ls framebuffer mismatch: %zu of %zu bytes "
                                 L"differ. First at $%04X: expected $%02X, "
                                 L"got $%02X.",
                            label,
                            mismatchCount,
                            kHgrPayloadSize,
                            static_cast<unsigned> (baseAddr + firstMismatch),
                            static_cast<unsigned> (expectedAtMismatch),
                            static_cast<unsigned> (actualAtMismatch));
                Assert::Fail (msg);
            }
        };

        VerifyPage (0x2000, hgrPayload,   L"HGR page 1 (cassowary)");
        VerifyPage (0x4000, bandsPayload, L"HGR page 2 (test bands)");

        // ----- Cycle through the 3 display modes with keystrokes -----
        Assert::IsNotNull (core.keyboard.get (), L"AppleKeyboard must be present");

        // Keystroke 1 -> mode 1 (HGR2 bands).
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (200'000ULL);
        Assert::IsTrue (ss->IsPage2 (),
            L"Mode 1 must enable PAGE2 (HGR test bands visible)");
        Assert::IsTrue (ss->IsHiresMode (),
            L"Mode 1 must keep HIRES on");

        // Keystroke 2 -> mode 2 (LoRes test). Copy stamps text page 1.
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (200'000ULL);
        Assert::IsFalse (ss->IsHiresMode (),
            L"Mode 2 must clear HIRES (LoRes is text-page graphics)");
        Assert::IsTrue (ss->IsGraphicsMode (),
            L"Mode 2 must keep TEXT off");
        Assert::IsFalse (ss->IsPage2 (),
            L"Mode 2 must clear PAGE2");

        // Spot-check the LoRes pattern landed in text page 1.
        for (size_t i = 0; i < kLoresPayloadSize; i++)
        {
            Byte  actual = core.bus->ReadByte (
                static_cast<Word> (0x0400 + i));
            Byte  e      = loresPayload[i];
            if (actual != e)
            {
                wchar_t  msg[256] = {};
                swprintf_s (msg, L"LoRes pattern copy mismatch at $%04X: "
                                 L"expected $%02X, got $%02X.",
                            static_cast<unsigned> (0x0400 + i),
                            static_cast<unsigned> (e),
                            static_cast<unsigned> (actual));
                Assert::Fail (msg);
            }
        }

        // Keystroke 3 -> past last mode -> JMP $E000 (Applesoft cold start).
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (50'000ULL);
        // Cold start runs through a bunch of ROM init before reaching the
        // BASIC ready prompt; what we really want to assert is that we're
        // executing somewhere in the Applesoft / monitor ROM ($D000+).
        Assert::IsTrue (core.cpu->GetPC () >= 0xD000,
            L"After cycling past last mode, demo must JMP into ROM "
            L"($D000+, typically the Applesoft cold start at $E000)");

        // Side effect: emit the .dsk alongside the source so the demo can
        // also be booted in the GUI without re-running the test. Best-
        // effort -- silent failure on read-only checkouts (CI).
        fs::path  dskOut = src.parent_path () / "casso-rocks.dsk";
        std::ofstream  out (dskOut, std::ios::binary);
        if (out)
        {
            out.write (reinterpret_cast<const char *> (raw.data ()),
                       static_cast<std::streamsize> (raw.size ()));
        }
    }
};
