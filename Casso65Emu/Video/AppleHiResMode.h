#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleHiResMode
//
//  280x192 hi-res graphics with NTSC color artifact simulation.
//
////////////////////////////////////////////////////////////////////////////////

class AppleHiResMode : public VideoOutput
{
public:
    explicit AppleHiResMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-hires"; }

private:
    static Word ScanlineAddress (int scanline, Word pageBase);

    MemoryBus & m_bus;
};
