#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"

class CharacterRomData;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextMode
//
//  80x24 text renderer using interleaved main/aux memory.
//
////////////////////////////////////////////////////////////////////////////////

class Apple80ColTextMode : public VideoOutput
{
public:
    explicit Apple80ColTextMode (MemoryBus & bus);
    Apple80ColTextMode (MemoryBus & bus, const CharacterRomData & charRom);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-text80"; }

    // Provide access to aux memory for 80-column interleaved rendering.
    // videoRam (passed to Render) is main RAM; aux is set separately.
    void SetAuxMemory (const Byte * auxMem) { m_auxMem = auxMem; }

private:
    MemoryBus              & m_bus;
    const CharacterRomData & m_charRom;
    const Byte             * m_auxMem = nullptr;
};
