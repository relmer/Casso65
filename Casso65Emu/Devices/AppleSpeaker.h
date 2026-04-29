#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleSpeaker
//
//  Speaker toggle at $C030. Each read toggles the speaker state and records
//  the CPU cycle timestamp for audio waveform generation.
//
////////////////////////////////////////////////////////////////////////////////

class AppleSpeaker : public MemoryDevice
{
public:
    AppleSpeaker ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC030; }
    Word GetEnd   () const override { return 0xC03F; }
    void Reset    () override;

    // Audio interface
    float GetSpeakerState () const { return m_speakerState; }

    const std::vector<uint32_t> & GetToggleTimestamps () const { return m_toggleTimestamps; }

    void ClearTimestamps () { m_toggleTimestamps.clear (); }

    void SetCycleCounter (uint64_t * pCycles) { m_pTotalCycles = pCycles; }

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    float                       m_speakerState;
    std::vector<uint32_t>       m_toggleTimestamps;
    uint64_t *                  m_pTotalCycles;
};
