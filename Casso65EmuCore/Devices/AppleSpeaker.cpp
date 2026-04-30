#include "Pch.h"

#include "AppleSpeaker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSpeaker
//
////////////////////////////////////////////////////////////////////////////////

AppleSpeaker::AppleSpeaker ()
    : m_speakerState    (-1.0f),
      m_frameInitialState (-1.0f),
      m_pTotalCycles    (nullptr),
      m_frameCycleStart (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleSpeaker::Read (Word address)
{
    UNREFERENCED_PARAMETER (address);

    // Toggle speaker state (use +/-1.0 to match audio pipeline expectations)
    m_speakerState = (m_speakerState > 0.0f) ? -1.0f : 1.0f;

    // Record frame-relative timestamp
    if (m_pTotalCycles != nullptr)
    {
        uint32_t relCycle = static_cast<uint32_t> (*m_pTotalCycles - m_frameCycleStart);
        m_toggleTimestamps.push_back (relCycle);
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleSpeaker::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (address);
    UNREFERENCED_PARAMETER (value);

    // Write also toggles (some software uses STA $C030)
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleSpeaker::Reset ()
{
    m_speakerState = -1.0f;
    m_toggleTimestamps.clear ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> AppleSpeaker::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return std::make_unique<AppleSpeaker> ();
}
