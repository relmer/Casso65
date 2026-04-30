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
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyboardTests)
{
public:

    TEST_METHOD (KeyPress_SetsBit7Strobe)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('A');
        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) != 0);
        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F));
    }

    TEST_METHOD (ReadC010_ClearsStrobe)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('B');
        kbd.Read (0xC010);

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0);
    }

    TEST_METHOD (LowercaseInput_ConvertedToUppercase)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('a');
        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F));
    }

    TEST_METHOD (NoKeyPressed_NoBit7)
    {
        AppleKeyboard kbd;

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0);
    }

    TEST_METHOD (SetKeyDown_ThenUp_DoesNotCrash)
    {
        AppleKeyboard kbd;

        kbd.SetKeyDown (true);
        kbd.KeyPress ('X');
        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) != 0);

        kbd.SetKeyDown (false);
        kbd.Read (0xC010);

        Byte val2 = kbd.Read (0xC010);

        Assert::IsTrue ((val2 & 0x80) == 0);
    }

    TEST_METHOD (IIeKeyboard_SetKeyDown_DoesNotCrash)
    {
        AppleIIeKeyboard kbd;

        kbd.SetKeyDown (true);
        kbd.KeyPress ('Z');
        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) != 0);

        kbd.SetKeyDown (false);
    }

    TEST_METHOD (IIeKeyboard_UpcastToBase_KeyInputWorks)
    {
        AppleIIeKeyboard iieKbd;
        AppleKeyboard * basePtr = &iieKbd;

        basePtr->SetKeyDown (true);
        basePtr->KeyPress ('A');
        Byte val = basePtr->Read (0xC000);

        Assert::IsTrue ((val & 0x80) != 0);
        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F));

        basePtr->SetKeyDown (false);
    }
};
