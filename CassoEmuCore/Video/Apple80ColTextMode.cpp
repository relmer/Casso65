#include "Pch.h"

#include "Apple80ColTextMode.h"
#include "CharacterRomData.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextMode
//
////////////////////////////////////////////////////////////////////////////////

// Singleton default char ROM (embedded fallback) for legacy single-arg constructor
static const CharacterRomData & GetDefaultCharRom()
{
    static CharacterRomData s_defaultRom;
    return s_defaultRom;
}

Apple80ColTextMode::Apple80ColTextMode (MemoryBus & bus)
    : m_bus     (bus),
      m_charRom (GetDefaultCharRom())
{
}

Apple80ColTextMode::Apple80ColTextMode (MemoryBus & bus, const CharacterRomData & charRom)
    : m_bus     (bus),
      m_charRom (charRom)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word Apple80ColTextMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x0800) : static_cast<Word> (0x0400);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  80-column text mode renders 80x24 characters. Columns alternate between
//  aux and main memory (aux = even columns, main = odd columns).
//  For now, this renders similarly to 40-col but at half width per char.
//
////////////////////////////////////////////////////////////////////////////////

void Apple80ColTextMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{
    Word pageBase = static_cast<Word> (0x0400);  // 80-col always uses page 1

    static constexpr int kTextCols   = 80;
    static constexpr int kTextRows   = 24;
    static constexpr int kCharWidth  = 7;
    static constexpr int kCharHeight = 8;

    static constexpr uint32_t kColorGreen = 0xFF00FF00;
    static constexpr uint32_t kColorBlack = 0xFF000000;

    // 80-column mode: even columns come from aux RAM, odd columns from main RAM.
    // Each "column" in screen memory is one 7-pixel-wide character.
    for (int row = 0; row < kTextRows; row++)
    {
        Word rowAddr = static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));

        for (int col = 0; col < kTextCols; col++)
        {
            int  memCol  = col / 2;
            bool fromAux = (col % 2) == 0;
            Word addr    = static_cast<Word> (rowAddr + memCol);

            Byte charCode = 0;

            if (fromAux && m_auxMem != nullptr)
            {
                charCode = m_auxMem[addr];
            }
            else if (videoRam != nullptr)
            {
                charCode = videoRam[addr];
            }
            else
            {
                charCode = m_bus.ReadByte (addr);
            }

            for (int py = 0; py < kCharHeight; py++)
            {
                Byte glyphRow = m_charRom.GetGlyphRow (charCode, py);

                for (int px = 0; px < kCharWidth; px++)
                {
                    bool     pixelOn = (glyphRow & (1 << px)) != 0;
                    uint32_t color   = pixelOn ? kColorGreen : kColorBlack;

                    int fbX = col * kCharWidth + px;
                    int fbY = row * kCharHeight * 2 + py * 2;

                    if (fbX < fbWidth && fbY + 1 < fbHeight)
                    {
                        framebuffer[fbY * fbWidth + fbX]       = color;
                        framebuffer[(fbY + 1) * fbWidth + fbX] = color;
                    }
                }
            }
        }
    }
}
