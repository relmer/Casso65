#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/AppleIIeKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardTests
//
//  Adversarial tests proving the keyboard actually works end-to-end.
//  Verifies ASCII encoding, strobe behavior, key overwrite semantics,
//  and bus integration.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyboardTests)
{
public:

    TEST_METHOD (KeyPress_A_ReadsC1WithHighBit)
    {
        // Apple II: 'A' = $41. With strobe bit 7 set: $C1.
        AppleKeyboard kbd;
        kbd.KeyPress ('A');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC1), val,
            L"Press 'A' -> $C000 should return $C1 (ASCII $41 | $80)");
    }

    TEST_METHOD (ReadC010_ClearsStrobeBit7)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('B');

        // Read $C010 to clear strobe
        kbd.Read (0xC010);

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"After strobe clear, $C000 should return $42 (no high bit)");
        Assert::IsTrue ((val & 0x80) == 0,
            L"Bit 7 should be clear after reading $C010");
    }

    TEST_METHOD (SecondKeyPress_OverwritesFirst)
    {
        // Press 'X', don't clear strobe, press 'Y'.
        // $C000 should show 'Y' with strobe, not 'X'.
        AppleKeyboard kbd;
        kbd.KeyPress ('X');
        kbd.KeyPress ('Y');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('Y'), static_cast<Byte> (val & 0x7F),
            L"Second key press should overwrite first");
        Assert::IsTrue ((val & 0x80) != 0,
            L"Strobe should still be set after second key press");
    }

    TEST_METHOD (LowercaseConverted_ToUppercase)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('a');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F),
            L"Lowercase 'a' should be converted to uppercase 'A'");
    }

    TEST_METHOD (AllPrintableASCII_MapCorrectly)
    {
        // Verify all printable ASCII characters produce correct output
        for (Byte ch = 0x20; ch <= 0x5F; ch++)
        {
            AppleKeyboard kbd;
            kbd.KeyPress (ch);

            Byte val = kbd.Read (0xC000);
            Byte asciiPart = val & 0x7F;

            Assert::AreEqual (ch, asciiPart,
                std::format (L"Key ${:02X} should read back as ${:02X}, got ${:02X}",
                    ch, ch, asciiPart).c_str ());
            Assert::IsTrue ((val & 0x80) != 0,
                std::format (L"Key ${:02X} should have strobe set", ch).c_str ());
        }
    }

    TEST_METHOD (ReturnKey_Produces0D)
    {
        AppleKeyboard kbd;
        kbd.KeyPress (0x0D);  // Return/Enter

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x0D), static_cast<Byte> (val & 0x7F),
            L"Return key should produce $0D");
        Assert::IsTrue ((val & 0x80) != 0,
            L"Return key should have strobe set");
    }

    TEST_METHOD (EscapeKey_Produces1B)
    {
        AppleKeyboard kbd;
        kbd.KeyPress (0x1B);  // Escape

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x1B), static_cast<Byte> (val & 0x7F),
            L"Escape key should produce $1B");
    }

    TEST_METHOD (NoKeyPressed_NoBit7)
    {
        AppleKeyboard kbd;

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0,
            L"No key pressed: bit 7 should be clear");
    }

    TEST_METHOD (StrobePersists_UntilC010Read)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('Z');

        // Multiple reads of $C000 should all have strobe set
        Byte val1 = kbd.Read (0xC000);
        Byte val2 = kbd.Read (0xC000);
        Byte val3 = kbd.Read (0xC000);

        Assert::IsTrue ((val1 & 0x80) != 0, L"1st read should have strobe");
        Assert::IsTrue ((val2 & 0x80) != 0, L"2nd read should have strobe");
        Assert::IsTrue ((val3 & 0x80) != 0, L"3rd read should have strobe");

        // Now clear and verify
        kbd.Read (0xC010);
        Byte val4 = kbd.Read (0xC000);
        Assert::IsTrue ((val4 & 0x80) == 0, L"After clear, no strobe");
    }

    TEST_METHOD (KeyPress_ViaBus_ReturnsCorrectValue)
    {
        // Prove keyboard works when accessed through MemoryBus
        MemoryBus bus;
        AppleKeyboard kbd;
        bus.AddDevice (&kbd);

        kbd.KeyPress ('H');
        Byte val = bus.ReadByte (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC8), val,
            L"'H' via bus should read $C8 at $C000");

        // Clear strobe via bus
        bus.ReadByte (0xC010);
        Byte val2 = bus.ReadByte (0xC000);
        Assert::IsTrue ((val2 & 0x80) == 0,
            L"Strobe should be clear after bus read of $C010");
    }

    TEST_METHOD (SetKeyDown_AffectsC010ReadValue)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('X');

        // Clear strobe
        kbd.Read (0xC010);

        // With key physically held, $C010 should show bit 7
        kbd.SetKeyDown (true);
        Byte val = kbd.Read (0xC010);
        Assert::IsTrue ((val & 0x80) != 0,
            L"$C010 should show bit 7 when key is physically held");

        kbd.SetKeyDown (false);
        Byte val2 = kbd.Read (0xC010);
        Assert::IsTrue ((val2 & 0x80) == 0,
            L"$C010 should not show bit 7 when key is released");
    }

    TEST_METHOD (IIeKeyboard_UpcastToBase_Works)
    {
        AppleIIeKeyboard iieKbd;
        AppleKeyboard * basePtr = &iieKbd;

        basePtr->KeyPress ('A');
        Byte val = basePtr->Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC1), val,
            L"IIe keyboard upcast should work identically to base");
    }

    TEST_METHOD (Reset_ClearsAllState)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('Z');
        kbd.SetKeyDown (true);

        kbd.Reset ();

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x00), val,
            L"After Reset, $C000 should return $00 (no key, no strobe)");
    }
};
