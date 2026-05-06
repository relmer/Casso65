#include "Pch.h"

#include "AppleIIeSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
////////////////////////////////////////////////////////////////////////////////

AppleIIeSoftSwitchBank::AppleIIeSoftSwitchBank (MemoryBus * bus)
    : AppleSoftSwitchBank (),
      m_bus               (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleIIeSoftSwitchBank::Read (Word address)
{
    bool bankingChange = false;

    // IIe-specific switches in $C00C-$C00F (80COL, ALTCHARSET).
    // Reads of these on real //e trigger the same side effect as writes.
    // ($C000/$C001 80STORE only triggers on writes, not reads.)
    switch (address)
    {
        case 0xC00C:
            m_80colMode = false;
            return 0;
        case 0xC00D:
            m_80colMode = true;
            return 0;
        case 0xC00E:
            m_altCharSet = false;
            return 0;
        case 0xC00F:
            m_altCharSet = true;
            return 0;
        case 0xC05E:
            m_doubleHiRes = true;
            bankingChange = true;
            break;
        case 0xC05F:
            m_doubleHiRes = false;
            bankingChange = true;
            break;
        default:
            break;
    }

    // PAGE1/PAGE2 ($C054/$C055) and HIRES/LORES ($C056/$C057) trigger banking
    // recalculation when 80STORE may be active.
    if (address >= 0xC054 && address <= 0xC057)
    {
        bankingChange = true;
    }

    Byte result = 0;

    // Fall through to base class for $C050-$C057 (graphics mode toggles)
    if (address >= 0xC050 && address <= 0xC057)
    {
        result = AppleSoftSwitchBank::Read (address);
    }

    if (bankingChange && m_bus != nullptr)
    {
        m_bus->NotifyBankingChanged ();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Per Apple //e Tech Ref:
//    $C000 write -> 80STORE OFF
//    $C001 write -> 80STORE ON
//    $C00C-$C00F writes -> same as reads (80COL, ALTCHARSET)
//    Other writes -> same as reads
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeSoftSwitchBank::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    if (address == 0xC000 || address == 0xC001)
    {
        bool newState     = (address == 0xC001);
        bool stateChanged = newState != m_80store;
        m_80store         = newState;

        if (stateChanged && m_bus != nullptr)
        {
            m_bus->NotifyBankingChanged ();
        }
        return;
    }

    // All other writes have the same effect as reads
    Read (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleIIeSoftSwitchBank::Reset ()
{
    AppleSoftSwitchBank::Reset ();
    m_80store     = false;
    m_80colMode   = false;
    m_doubleHiRes = false;
    m_altCharSet  = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleIIeSoftSwitchBank::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<AppleIIeSoftSwitchBank> (&bus);
}
