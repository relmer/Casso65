#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





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

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-text80"; }

private:
    MemoryBus & m_bus;
};
