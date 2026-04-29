#include "Pch.h"

#include "AppleDoubleHiResMode.h"
#include "AppleHiResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
//  560x192 double hi-res using interleaved aux/main memory.
//  For now renders as standard hi-res (proper double hi-res requires
//  aux memory access that will be wired via AuxRamCard).
//
////////////////////////////////////////////////////////////////////////////////

AppleDoubleHiResMode::AppleDoubleHiResMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleDoubleHiResMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x4000) : static_cast<Word> (0x2000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Double hi-res: 560x192, 16 colors from alternating aux/main bytes.
//  For initial implementation, renders same as standard hi-res.
//
////////////////////////////////////////////////////////////////////////////////

void AppleDoubleHiResMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{
    UNREFERENCED_PARAMETER (videoRam);

    // Placeholder: fill with black for now
    memset (framebuffer, 0, static_cast<size_t> (fbWidth) * fbHeight * sizeof (uint32_t));
}
