#include "Pch.h"

#include "VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Phase 4 stub: accumulates CPU cycles into the running counter. Phase 5
//  will wrap modulo 17,030 (kCyclesPerFrame) and gate IsInVblank on
//  scanlines 192..261; until then the counter is observation-only.
//
////////////////////////////////////////////////////////////////////////////////

void VideoTiming::Tick (uint32_t cpuCycles)
{
    m_cycleCounter += cpuCycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Same effect as SoftReset — the timing model has no DRAM-shaped state.
//
////////////////////////////////////////////////////////////////////////////////

void VideoTiming::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    SoftReset ();
}
