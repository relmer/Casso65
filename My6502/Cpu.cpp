#include "Pch.h"

#include "Cpu.h"



void Cpu::Reset ()
{
    pc = 0xFFFC;
    sp = 0x0100;

    status.status = 0;

    a = 0;
    x = 0;
    y = 0;

    memset (memory, 0, sizeof (memory));
}



void Cpu::Run ()
{
    do
    {
        byte opcode         = memory[pc];
        byte instruction    = Decode (opcode); 
    }
    while (0);
}



word Cpu::Decode (byte opcode)
{
    return word ();
}



byte Cpu::FetchInstruction ()
{
    return byte ();
}
