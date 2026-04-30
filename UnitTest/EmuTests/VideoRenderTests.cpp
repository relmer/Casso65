#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Video/AppleTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoRenderWithMemoryTests
//
//  Tests that verify video renderers work correctly when passed a real
//  videoRam pointer (e.g. Cpu::GetMemory()), not just nullptr.
//  These tests would have caught the "green screen" bug where renderers
//  received nullptr for videoRam and silently rendered garbage.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoRenderWithMemoryTests)
{
public:

    TEST_METHOD (TextMode_RenderWithMemory_ProducesNonBlackOutput)
    {
        // Create a MemoryBus + RAM, write $C1 (normal 'A') to text page $0400,
        // then render using a memory array (not nullptr).
        // Verify framebuffer is NOT all-black — this catches the "nullptr
        // videoRam" bug where the renderer read from a zeroed pointer.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Prepare a 64KB memory image with a visible character at $0400
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x0400] = 0xC1;  // Normal 'A'

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFF000000u);

        textMode.Render (mem.data (), fb.data (), fbW, fbH);

        // At least one pixel must NOT be black
        bool hasNonBlack = false;

        for (int i = 0; i < fbW * fbH && !hasNonBlack; i++)
        {
            if (fb[i] != 0xFF000000u)
            {
                hasNonBlack = true;
            }
        }

        Assert::IsTrue (hasNonBlack,
            L"Rendering with memory pointer should produce non-black output");
    }

    TEST_METHOD (TextMode_RenderWithNullMemory_DoesNotCrash)
    {
        // Calling Render with nullptr videoRam should fall back to bus reads,
        // not crash.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        // Should not crash — reads from bus when videoRam is nullptr
        textMode.Render (nullptr, fb.data (), fbW, fbH);

        // Verify it actually rendered something (green pixels for 'A')
        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen,
            L"Null videoRam should fall back to bus and still render");
    }

    TEST_METHOD (TextMode_NormalCharacter_RendersGreenPixels)
    {
        // Write $C1 (normal 'A') into a memory array at $0400, render,
        // verify some green pixels exist in the first character cell.
        std::vector<Byte> mem (0x10000, 0x00);

        // Fill text page with spaces so only our character stands out
        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            mem[addr] = 0xA0;  // Normal space
        }

        mem[0x0400] = 0xC1;  // Normal 'A'

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (mem.data (), fb.data (), fbW, fbH);

        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen,
            L"Normal 'A' rendered with memory pointer should have green pixels");
    }

    TEST_METHOD (TextMode_InverseCharacter_RendersAllGreen)
    {
        // Write $00 (inverse '@') at $0400 via memory pointer, render, verify
        // that the background pixels in that cell are all green (inverse mode
        // inverts the glyph so "off" pixels become green).
        std::vector<Byte> mem (0x10000, 0xA0);  // Fill with normal spaces

        mem[0x0400] = 0x20;  // Inverse space ($20 = space in inverse range)

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (mem.data (), fb.data (), fbW, fbH);

        // Inverse space: all pixels in the character cell should be green
        // because space has no "on" bits and inverse flips them all
        bool allGreen = true;

        for (int y = 0; y < 16 && allGreen; y++)
        {
            for (int x = 0; x < 14 && allGreen; x++)
            {
                if (fb[y * fbW + x] != 0xFF00FF00u)
                {
                    allGreen = false;
                }
            }
        }

        Assert::IsTrue (allGreen,
            L"Inverse space should render all green in the character cell");
    }

    TEST_METHOD (TextMode_RowAddressCalculation)
    {
        // Verify all 24 rows use the correct interleaved addresses:
        //   base + 128*(row%8) + 40*(row/8)
        // Row 0  = $0400, Row 1  = $0480, Row 8  = $0428, etc.
        struct RowAddr
        {
            int  row;
            Word expected;
        };

        RowAddr cases[] =
        {
            {  0, 0x0400 },
            {  1, 0x0480 },
            {  2, 0x0500 },
            {  3, 0x0580 },
            {  4, 0x0600 },
            {  5, 0x0680 },
            {  6, 0x0700 },
            {  7, 0x0780 },
            {  8, 0x0428 },
            {  9, 0x04A8 },
            { 10, 0x0528 },
            { 16, 0x0450 },
            { 23, 0x07D0 },
        };

        std::vector<Byte> mem (0x10000, 0xA0);

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Write a unique character at each expected row address
        for (const auto & tc : cases)
        {
            mem[tc.expected] = 0xC1;  // Normal 'A'
        }

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (mem.data (), fb.data (), fbW, fbH);

        // Verify each row's col-0 character cell has green pixels
        for (const auto & tc : cases)
        {
            int fbY = tc.row * 8 * 2;  // Each row: 8 pixels * 2x scale
            bool hasGreen = false;

            for (int y = fbY; y < fbY + 16 && !hasGreen; y++)
            {
                for (int x = 0; x < 14 && !hasGreen; x++)
                {
                    if (fb[y * fbW + x] == 0xFF00FF00u)
                    {
                        hasGreen = true;
                    }
                }
            }

            Assert::IsTrue (hasGreen,
                std::format (L"Row {} (address ${:04X}) should have green pixels",
                    tc.row, tc.expected).c_str ());
        }
    }

    TEST_METHOD (LoRes_RenderWithMemory_ProducesColorOutput)
    {
        // Write non-black color data to text page via memory pointer, render
        // in lo-res mode, verify non-black output.
        std::vector<Byte> mem (0x10000, 0x00);

        mem[0x0400] = 0xFF;  // Color 15 (white) top and bottom

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFF000000u);

        lores.Render (mem.data (), fb.data (), fbW, fbH);

        // First pixel should be white (color 15 = 0xFFFFFFFF)
        Assert::AreEqual (0xFFFFFFFFu, fb[0],
            L"Lo-res with memory pointer should render white for color $FF");
    }

    TEST_METHOD (HiRes_RenderWithMemory_ProducesOutput)
    {
        // Write non-zero data to hi-res page ($2000-$3FFF) via memory pointer,
        // render, verify non-black output.
        std::vector<Byte> mem (0x10000, 0x00);

        // Set bits at scanline 0, byte 0 — two adjacent pixels for white
        mem[0x2000] = 0x03;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFF000000u);

        hires.Render (mem.data (), fb.data (), fbW, fbH);

        // Adjacent pixels render white
        Assert::AreEqual (0xFFFFFFFFu, fb[0],
            L"Hi-res with memory pointer should render white for adjacent pixels");
    }
};
