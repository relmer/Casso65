#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/LanguageCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardTests
//
//  Adversarial tests proving bank switching ACTUALLY redirects memory reads
//  and writes, not just that flag getters return the right values.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LanguageCardTests)
{
public:

    TEST_METHOD (InitialState_ReadRomNotRam)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        Assert::IsFalse (lc.IsReadRam (), L"Initial state must read ROM");
        Assert::IsFalse (lc.IsWriteRam (), L"Initial state must not write RAM");
    }

    TEST_METHOD (ReadC080_EnablesRamRead)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsReadRam (),
            L"$C080 must enable reading from LC RAM");
    }

    TEST_METHOD (ReadD000_InitialState_ReturnsRomData)
    {
        // With LC in initial state, reading $D000 through LanguageCardBank
        // should return ROM data, not LC RAM (which is zero).
        MemoryBus bus;
        LanguageCard lc (bus);

        std::vector<Byte> rom (0x3000, 0x00);
        rom[0x0000] = 0xAA;  // $D000 in ROM = $AA
        rom[0x1000] = 0xBB;  // $E000 in ROM = $BB
        lc.SetRomData (rom);

        LanguageCardBank bank (lc);

        Byte val = bank.Read (0xD000);
        Assert::AreEqual (static_cast<Byte> (0xAA), val,
            L"Initial state: $D000 should return ROM data $AA");

        Byte val2 = bank.Read (0xE000);
        Assert::AreEqual (static_cast<Byte> (0xBB), val2,
            L"Initial state: $E000 should return ROM data $BB");
    }

    TEST_METHOD (ReadRamEnabled_ReturnsLCRam_NotRom)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        std::vector<Byte> rom (0x3000, 0xEA);
        lc.SetRomData (rom);

        // Enable read RAM
        lc.Read (0xC080);

        LanguageCardBank bank (lc);
        Byte val = bank.Read (0xD000);

        // LC RAM is initialized to zero, ROM is $EA
        Assert::AreEqual (static_cast<Byte> (0x00), val,
            L"Read-RAM enabled: $D000 should return LC RAM (0), not ROM ($EA)");
    }

    TEST_METHOD (WriteRam_PersistsAfterReenabling)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Enable write (double-read odd switch)
        lc.Read (0xC081);
        lc.Read (0xC081);
        Assert::IsTrue (lc.IsWriteRam (), L"Write should be enabled");

        // Write to LC RAM
        lc.WriteRam (0xD000, 0x42);

        // Switch to ROM read
        lc.Read (0xC082);  // Even switch = read RAM, but also resets write
        Assert::IsFalse (lc.IsWriteRam (), L"Even switch should disable write");

        // Re-enable read RAM
        lc.Read (0xC080);

        // Data should persist
        Byte val = lc.ReadRam (0xD000);
        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"Written LC RAM data must persist after re-enabling read");
    }

    TEST_METHOD (DoubleRead_OddSwitch_RequiredForWriteEnable)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Single read: write NOT enabled
        lc.Read (0xC081);
        Assert::IsFalse (lc.IsWriteRam (),
            L"Single read of $C081 must NOT enable write");

        // Second read of SAME switch: write enabled
        lc.Read (0xC081);
        Assert::IsTrue (lc.IsWriteRam (),
            L"Double read of $C081 must enable write");
    }

    TEST_METHOD (DoubleRead_DifferentSwitches_DoesNotEnableWrite)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Read $C081 then $C083 — different switch addresses
        lc.Read (0xC081);
        lc.Read (0xC083);

        Assert::IsFalse (lc.IsWriteRam (),
            L"Double read of DIFFERENT odd switches must NOT enable write");
    }

    TEST_METHOD (Bank2Selected_ByDefault)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsBank2 (),
            L"Bank 2 should be selected by default");
    }

    TEST_METHOD (ReadC088_SelectsBank1)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC088);

        Assert::IsFalse (lc.IsBank2 (),
            L"$C088 should select bank 1 (not bank 2)");
    }

    TEST_METHOD (Bank1_WritesIsolatedFromBank2)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Write $AA to bank 2 at $D000
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xAA);

        // Switch to bank 1 and write $BB
        lc.Read (0xC089);
        lc.Read (0xC089);
        lc.WriteRam (0xD000, 0xBB);

        // Read bank 1
        lc.Read (0xC088);
        Byte bank1Val = lc.ReadRam (0xD000);

        // Switch back to bank 2
        lc.Read (0xC080);
        Byte bank2Val = lc.ReadRam (0xD000);

        Assert::AreEqual (static_cast<Byte> (0xBB), bank1Val,
            L"Bank 1 $D000 should contain $BB");
        Assert::AreEqual (static_cast<Byte> (0xAA), bank2Val,
            L"Bank 2 $D000 should still contain $AA (isolated from bank 1)");
    }

    TEST_METHOD (UpperRam_E000_FFFF_SharedAcrossBanks)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Write to $E000 with bank 2 selected
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xE000, 0xCC);

        // Switch to bank 1
        lc.Read (0xC089);
        lc.Read (0xC089);

        // $E000-$FFFF is shared, not banked
        Byte val = lc.ReadRam (0xE000);
        Assert::AreEqual (static_cast<Byte> (0xCC), val,
            L"$E000 should be same regardless of bank selection");
    }

    TEST_METHOD (LanguageCardBank_Write_GoesToLCRam)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);

        LanguageCardBank bank (lc);
        bank.Write (0xE000, 0x42);

        Byte val = lc.ReadRam (0xE000);
        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"LanguageCardBank::Write must route to LC RAM");
    }

    TEST_METHOD (LanguageCardBank_WriteDisabled_Ignores)
    {
        MemoryBus bus;
        LanguageCard lc (bus);
        // Write is NOT enabled (initial state)

        LanguageCardBank bank (lc);
        bank.Write (0xD000, 0xFF);

        // Enable read RAM to check
        lc.Read (0xC080);
        Byte val = lc.ReadRam (0xD000);

        Assert::AreEqual (static_cast<Byte> (0x00), val,
            L"Write should be ignored when write is not enabled");
    }

    TEST_METHOD (Reset_ClearsStateAndRAM)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xFF);

        lc.Reset ();

        Assert::IsFalse (lc.IsReadRam (), L"Reset must clear readRam");
        Assert::IsFalse (lc.IsWriteRam (), L"Reset must clear writeRam");
        Assert::AreEqual (static_cast<Byte> (0), lc.ReadRam (0xD000),
            L"Reset must zero LC RAM");
    }

    TEST_METHOD (ReadRom_WithNoRomData_Returns0xFF)
    {
        MemoryBus bus;
        LanguageCard lc (bus);
        // No SetRomData called

        Byte val = lc.ReadRom (0xD000);
        Assert::AreEqual (static_cast<Byte> (0xFF), val,
            L"ReadRom with no ROM data should return $FF");
    }
};
