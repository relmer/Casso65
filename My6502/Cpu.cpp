#include "Pch.h"

#include "Cpu.h"
#include "CpuOperations.h"
#include "Utils.h"



Cpu::Cpu ()
{
    InitializeInstructionSet ();
}



void Cpu::Reset ()
{
    /*
    PC = 0xFFFC;
    SP = 0x0100;
    */

    status.status = 0;

    A = 0;
    X = 0;
    Y = 0;

    memset (memory, 0, sizeof (memory));




    // Test code
    PC = 0x8000;
    SP = 0x0100;

    memory[0x8000] = 0x09;  // ORA
    memory[0x8001] = 0x0F;  // Immediate
    memory[0x8002] = 0x09;  // ORA
    memory[0x8003] = 0xF0;  // Immediate
    // A should be FF after these instructions
}



void Cpu::Run ()
{
    do
    {
        Byte        opcode      = memory[PC++];
        Instruction instruction = instructionSet[opcode].instruction;
        Word        operand     = 0;

        if (!instructionSet[opcode].isLegal)
        {
            std::printf ("PC = %04X:  Skipping illegal instruction %02X\n", PC, opcode);
            continue;
        }

        operand = FetchOperand (instruction);
        ExecuteInstruction (instructionSet[opcode], operand);
    }
    while (1);
}



void Cpu::InitializeInstructionSet ()
{
    InitializeGroup01 ();

    PrintInstructionSet ();
}



void Cpu::InitializeGroup01 ()
{
    struct TableEntry
    {
        Group01::Opcode        opcode;
        Byte                   addressingModeFlags;
        Microcode::Operation   operation;
        Byte                 * pRegisterAffected;
    };

    TableEntry table[] =
    {
        { Group01::ORA, Group01::__AMF_AllModes,                             Microcode::Or,                &A      },
        { Group01::AND, Group01::__AMF_AllModes,                             Microcode::And,               &A      },
        { Group01::EOR, Group01::__AMF_AllModes,                             Microcode::Xor,               &A      },
        { Group01::ADC, Group01::__AMF_AllModes,                             Microcode::AddWithCarry,      &A      },
        { Group01::STA, Group01::__AMF_AllModes & ~(Group01::AMF_Immediate), Microcode::Store,             &A      },
        { Group01::LDA, Group01::__AMF_AllModes,                             Microcode::Load,              &A      },
        { Group01::CMP, Group01::__AMF_AllModes,                             Microcode::Compare,           nullptr },
        { Group01::SBC, Group01::__AMF_AllModes,                             Microcode::SubtractWithCarry, &A      },
    };


    for (TableEntry entry : table)
    { 
        CreateGroup01Instruction (entry.opcode, entry.addressingModeFlags, entry.operation, entry.pRegisterAffected);
    }
}



void Cpu::CreateGroup01Instruction (Group01::Opcode        opcode, 
                                    Byte                   addressingModeFlags, 
                                    Microcode::Operation   operation, 
                                    Byte                 * pRegisterAffected)
{
    Byte addressingMode            = Group01::__AM_First;
    Byte currentAddressingModeFlag = 1;

    while (addressingMode < Group01::__AM_Count)
    {
        if (addressingModeFlags & currentAddressingModeFlag)
        {
            Instruction instruction = Group01::CreateInstruction (opcode, (Group01::AddressingMode) addressingMode);
            instructionSet[instruction.asByte] = Microcode (instruction, operation, pRegisterAffected);
        }

        ++addressingMode;
        currentAddressingModeFlag <<= 1;
    }
}



void Cpu::PrintInstructionSet ()
{
    for (size_t i = 0; i < ARRAYSIZE(instructionSet); i++)
    {
        if (instructionSet[i].isLegal)
        {
            Byte opcode = instructionSet[i].instruction.asBits.opcode;
            Byte addressingMode = instructionSet[i].instruction.asBits.addressingMode;

            std::printf ("Instruction %02X:  %s ($%02X) %s\n",
                         (unsigned int) i,
                         Group01::opcodeName[opcode],
                         instructionSet[i].instruction.asByte,
                         Group01::addressingModeName[addressingMode]);
        }
    }
}



Word Cpu::FetchOperand (Instruction instruction)
{
    switch (instruction.asBits.addressingMode)
    {
        /*
            case Group01::AM_ZeroPageXIndirect:
                return memory[(memory[PC++] + X) & 0xFF];

            case Group01::AM_ZeroPage:
                return memory[memory[PC++]];
        */

    case Group01::AM_Immediate:
        return memory[PC++];

        /*
            case Group01::AM_Absolute:
                return memory[ReadWord (PC)];

            case Group01::AM_ZeroPageIndirectY:
                return memory[(memory[PC++] + Y) & 0xFF];

            case Group01::AM_ZeroPageX:
                return memory[(memory[PC++] + Y) & 0xFF];

            case Group01::AM_AbsoluteY:
                return memory[ReadWord (PC) + Y];

            case Group01::AM_AbsoluteX:
                return memory[ReadWord (PC) + X];

            }
        */
    default:
        std::printf ("Unhandled addressing mode %d\n", instruction.asBits.addressingMode);
        return 0;
    }
}



void Cpu::ExecuteInstruction (Microcode microcode, Word operand)
{
    switch (microcode.operation)
    {
    case Microcode::Or:
        CpuOperations::Or (microcode.pRegisterAffected, (Byte) operand);
        break;
    }
}


