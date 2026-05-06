#pragma once

#include "Pch.h"
#include "Cpu6502.h"
#include "MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpu
//
//  Subclass of Cpu6502 that routes memory access through MemoryBus.
//  Inherits ICpu + I6502DebugInfo via Cpu6502; the strategy/composition
//  refactor in T028 widens this surface further.
//
////////////////////////////////////////////////////////////////////////////////

class EmuCpu : public Cpu6502
{
public:
    explicit EmuCpu (MemoryBus & memoryBus);

    // Override memory operations to route through MemoryBus
    Byte ReadByte  (Word address) override;
    void WriteByte (Word address, Byte value) override;
    Word ReadWord  (Word address) override;
    void WriteWord (Word address, Word value) override;

    // Cycle tracking — backed by the Cpu6502::m_totalCycles counter so
    // the existing Add/Reset/Get surface and the new ICpu::GetCycleCount
    // observe the same value.
    uint64_t   GetTotalCycles      () const { return m_totalCycles; }
    void       ResetCycles         ()       { m_totalCycles = 0; }
    uint64_t * GetCycleCounterPtr  ()       { return &m_totalCycles; }
    void       AddCycles           (Byte n) { m_totalCycles += n; }

    // Initialize for emulation: set SP and fetch reset vector
    void InitForEmulation ();

private:
    MemoryBus & m_memoryBus;
};
