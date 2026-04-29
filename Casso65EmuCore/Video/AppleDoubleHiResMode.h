#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
//  560x192 double hi-res from interleaved main/aux hi-res memory.
//
////////////////////////////////////////////////////////////////////////////////

class AppleDoubleHiResMode : public VideoOutput
{
public:
    explicit AppleDoubleHiResMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-doublehires"; }

private:
    MemoryBus & m_bus;
};
