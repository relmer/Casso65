#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/LanguageCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LanguageCardTests)
{
public:

    TEST_METHOD (InitialState_RomVisible)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        Assert::IsFalse (lc.IsReadRam ());
        Assert::IsFalse (lc.IsWriteRam ());
    }

    TEST_METHOD (ReadC080_EnablesRamRead)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsReadRam ());
    }

    TEST_METHOD (DoubleReadC081_EnablesRamWrite)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);

        Assert::IsTrue (lc.IsWriteRam ());
    }

    TEST_METHOD (SingleReadC081_DoesNotEnableWrite)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);

        Assert::IsFalse (lc.IsWriteRam ());
    }

    TEST_METHOD (Bank2Selected_ByDefault)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsBank2 ());
    }

    TEST_METHOD (ReadC088_SelectsBank1)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC088);

        Assert::IsFalse (lc.IsBank2 ());
    }

    TEST_METHOD (WriteRam_RequiresWriteEnable)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Enable write
        lc.Read (0xC081);
        lc.Read (0xC081);

        // Write to Language Card RAM
        lc.WriteRam (0xD000, 0xAB);
        Byte val = lc.ReadRam (0xD000);

        Assert::AreEqual (static_cast<Byte> (0xAB), val);
    }

    TEST_METHOD (Reset_ClearsState)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xFF);

        lc.Reset ();

        Assert::IsFalse (lc.IsReadRam ());
        Assert::IsFalse (lc.IsWriteRam ());
        Assert::AreEqual (static_cast<Byte> (0), lc.ReadRam (0xD000));
    }

    TEST_METHOD (LanguageCardBank_InitialState_ReadsRom)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        std::vector<Byte> rom (0x3000, 0xEA);
        lc.SetRomData (rom);

        LanguageCardBank bank (lc);

        Assert::AreEqual (static_cast<Byte> (0xEA), bank.Read (0xD000));
    }

    TEST_METHOD (LanguageCardBank_ReadRamEnabled_ReadsRam)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        std::vector<Byte> rom (0x3000, 0xEA);
        lc.SetRomData (rom);

        // Enable read RAM
        lc.Read (0xC080);

        // Write to LC RAM
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xAB);

        // Now enable read RAM again
        lc.Read (0xC080);

        LanguageCardBank bank (lc);

        Assert::AreEqual (static_cast<Byte> (0xAB), bank.Read (0xD000));
    }

    TEST_METHOD (LanguageCardBank_WriteEnabled_WritesToRam)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Enable write via double-read of odd switch
        lc.Read (0xC081);
        lc.Read (0xC081);

        LanguageCardBank bank (lc);
        bank.Write (0xE000, 0x42);

        Byte val = lc.ReadRam (0xE000);

        Assert::AreEqual (static_cast<Byte> (0x42), val);
    }
};
