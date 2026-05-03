#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VideoOutput
//
//  Abstract interface for video mode renderers.
//
////////////////////////////////////////////////////////////////////////////////

class VideoOutput
{
public:
    virtual ~VideoOutput () = default;

    virtual void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) = 0;

    virtual Word GetActivePageAddress (bool page2) const = 0;

    virtual const char * GetModeName () const = 0;

    // Video mode state set by the shell before Render
    void SetPage2 (bool page2) { m_page2 = page2; }
    bool IsPage2  () const     { return m_page2; }

protected:
    bool m_page2 = false;
};