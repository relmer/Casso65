#pragma once

#include "Pch.h"
#include "IVideoTiming.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoTiming
//
//  Phase 4 stub. Owns the cycle-in-frame counter so the EmulatorShell
//  reset paths have a concrete object to forward to. Phase 5 will fill in
//  the NTSC 17,030-cycle wraparound + scanline 192..261 VBL window per
//  the contract in IVideoTiming.h. For now Tick() advances the counter
//  but IsInVblank() returns false unconditionally, so $C019 reads
//  continue to surface the Phase-3 fallback (no behaviour change).
//
////////////////////////////////////////////////////////////////////////////////

class VideoTiming : public IVideoTiming
{
public:
                             VideoTiming () = default;
                             ~VideoTiming () override = default;

    void          Tick            (uint32_t cpuCycles) override;
    bool          IsInVblank      () const override { return false; }
    uint32_t      GetCycleInFrame () const override { return m_cycleCounter; }
    void          Reset           () override { m_cycleCounter = 0; }

    // Phase 4 split-reset (FR-034 / FR-035, data-model.md line 326).
    // Both paths clear the cycle counter; PowerCycle accepts a Prng for
    // signature symmetry but does not consume any randomness — there is
    // no DRAM-shaped state on the timing model.
    void          SoftReset       () { m_cycleCounter = 0; }
    void          PowerCycle      (Prng & prng);

private:
    uint32_t      m_cycleCounter = 0;
};
