#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootRomDecoderTests
//
//  End-to-end spec-conformance tests for the disk pipeline. Builds a
//  synthetic .dsk image whose track 0 sectors carry known data patterns,
//  nibblizes it through NibblizationLayer, mounts it on a //e + Disk II
//  harness, then runs the *actual* disk2.rom boot firmware on the
//  emulated 6502 until it has loaded sector 0 into RAM. Asserts that the
//  bytes the real boot ROM decoded into RAM match the original raw
//  source bytes.
//
//  These tests fail loudly for any mismatch between Casso's nibblizer
//  output and the spec the real Apple firmware expects to read --
//  specifically the kind of "self-symmetric round-trip passes but the
//  on-disk format is non-standard" bug that lets DOS 3.3 boot but every
//  RWTS sector read silently fail.
//
//  Test scaffolding details:
//    - The boot ROM (slot 6 firmware at $C600..$C6FF) reads sector 0 of
//      track 0 into $0800..$08FF, then JMPs to $0801. We let the CPU
//      run a generous cycle budget (~4M = ~4 seconds emulated time at
//      1 MHz; about 200 ms wall time at debug-build interpreter speed)
//      and then check the RAM contents.
//    - We use a sentinel pattern (each byte = its index) so any bit-
//      level corruption shows up as a localized mismatch we can inspect.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr Word       kBootRomEntry        = 0xC600;
    static constexpr Word       kIntCxRomOff         = 0xC006;
    static constexpr Word       kBootLoadAddress     = 0x0800;
    static constexpr int        kSlot6               = 6;
    static constexpr int        kDrive1              = 0;
    static constexpr uint64_t   kSectorReadCycles    = 4'000'000ULL;
    static constexpr size_t     kSectorBytes         = 256;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Build a 143360-byte raw .dsk image whose track-0 sector-0 contains
    //  a deterministic byte pattern. Every other sector stays zero so any
    //  off-by-one in track or sector indexing surfaces as wrong-byte
    //  failures rather than just no-op success.
    //
    ////////////////////////////////////////////////////////////////////////////

    vector<Byte> BuildSentinelDsk (Byte (*patternFn) (size_t))
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        size_t         i   = 0;

        // .dsk physical layout: track N sector N starts at offset
        // (track * 16 + sector_physical) * 256. The DOS 3.3 to-physical
        // map kDsk_LtoP starts with logical 0 -> physical 0, so DOS
        // logical sector 0 of track 0 lives at file offset 0.
        for (i = 0; i < kSectorBytes; i++)
        {
            raw[i] = patternFn (i);
        }

        return raw;
    }


    Byte PatternIdentity (size_t index)
    {
        return static_cast<Byte> (index & 0xFF);
    }


    Byte PatternComplement (size_t index)
    {
        return static_cast<Byte> (~static_cast<Byte> (index & 0xFF));
    }


    Byte PatternMixed (size_t index)
    {
        // Mix high and low bits to exercise the 6+2 group splitting:
        // bits 0..1 land in the "third group" nibbles; bits 2..7 land
        // in the main encoded run. A pattern that varies all 8 bits
        // independently catches partial-bit-group corruption.
        return static_cast<Byte> ((index * 0xA5u) ^ (index >> 3));
    }


    DiskImage * MountSentinelDsk (
        HeadlessHost  &  host,
        EmulatorCore  &  core,
        const vector<Byte> & raw)
    {
        HRESULT      hr        = S_OK;
        DiskImage *  external  = nullptr;

        hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
                                             "boot-rom-decoder.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, external);

        // Surface the disk2.rom slot ROM at $C600 (CxxxRomRouter /
        // INTCXROM=0; audit C1).
        core.bus->WriteByte (kIntCxRomOff, 0);

        core.cpu->SetPC (kBootRomEntry);
        return external;
    }


    void RunBootSectorRead (EmulatorCore & core, vector<Byte> & outRamCopy)
    {
        size_t   i = 0;

        core.RunCycles (kSectorReadCycles);

        outRamCopy.resize (kSectorBytes);
        for (i = 0; i < kSectorBytes; i++)
        {
            outRamCopy[i] = core.bus->ReadByte (
                static_cast<Word> (kBootLoadAddress + i));
        }
    }


    void AssertSectorMatches (const vector<Byte> & expected,
                              const vector<Byte> & ramCopy,
                              const wchar_t      * patternName)
    {
        size_t        firstMismatch = (size_t) -1;
        size_t        mismatchCount = 0;
        size_t        i             = 0;
        wchar_t       msg[512]      = {};

        Assert::AreEqual (expected.size (), ramCopy.size (), L"size mismatch");

        for (i = 0; i < expected.size (); i++)
        {
            if (expected[i] != ramCopy[i])
            {
                if (firstMismatch == (size_t) -1)
                {
                    firstMismatch = i;
                }
                mismatchCount++;
            }
        }

        if (mismatchCount > 0)
        {
            swprintf_s (msg,
                L"Pattern %ls: boot ROM decoded %zu/%zu bytes incorrectly. "
                L"First mismatch at byte %zu: expected $%02X, got $%02X. "
                L"This means Casso's nibblizer output does not match the "
                L"on-disk format the real Apple disk2.rom firmware decodes.",
                patternName,
                mismatchCount, expected.size (),
                firstMismatch,
                expected[firstMismatch],
                ramCopy[firstMismatch]);

            Assert::Fail (msg);
        }
    }
}





TEST_CLASS (BootRomDecoderTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec test: identity pattern (each byte = its index 0..255).
    //  Catches single-bit corruption anywhere in the data field.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootRom_Decodes_IdentityPattern_From_Sector0)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw      = BuildSentinelDsk (PatternIdentity);
        vector<Byte>   expected (raw.begin (), raw.begin () + kSectorBytes);
        vector<Byte>   ramCopy;

        MountSentinelDsk (host, core, raw);
        RunBootSectorRead (core, ramCopy);
        AssertSectorMatches (expected, ramCopy, L"identity (i)");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec test: complement pattern. Any bit-flip during decode shows up
    //  as a different mismatch position than the identity test above.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootRom_Decodes_ComplementPattern_From_Sector0)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw      = BuildSentinelDsk (PatternComplement);
        vector<Byte>   expected (raw.begin (), raw.begin () + kSectorBytes);
        vector<Byte>   ramCopy;

        MountSentinelDsk (host, core, raw);
        RunBootSectorRead (core, ramCopy);
        AssertSectorMatches (expected, ramCopy, L"complement (~i)");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Spec test: mixed-bit pattern. Stresses the 6+2 group splitting
    //  with values whose low 2 bits and high 6 bits vary independently
    //  -- the third-group / encoded-run separation in the nibblizer is
    //  the most common source of "passes round-trip but doesn't match
    //  Apple spec" bugs.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootRom_Decodes_MixedPattern_From_Sector0)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw      = BuildSentinelDsk (PatternMixed);
        vector<Byte>   expected (raw.begin (), raw.begin () + kSectorBytes);
        vector<Byte>   ramCopy;

        MountSentinelDsk (host, core, raw);
        RunBootSectorRead (core, ramCopy);
        AssertSectorMatches (expected, ramCopy, L"mixed (i*A5 ^ i>>3)");
    }
};
