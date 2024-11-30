#pragma once

#include "CpuStatus.h"
#include "Group00.h"
#include "Group01.h"
#include "Microcode.h"



class Cpu
{
    friend class CpuOperations;

public:
    Cpu ();
    void Reset ();
    void Run ();

protected:
    struct OperandInfo
    {
        Word location;
        Word effectiveAddress;
        Word operand;
    };

    void PrintSingleStepInfo    (Word initialPC, Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandAndComment (Byte opcode, const OperandInfo & operandInfo);
    void PrintOperandBytes      (Word initialPC, Byte opcode);
    void FetchOperand           (Microcode microcode, OperandInfo & operandInfo);
    void ExecuteInstruction     (Microcode microcode, const OperandInfo & operandInfo);

    void InitializeInstructionSet ();
    void InitializeGroup00 ();
    void InitializeGroup01 ();
    void CreateGroup01Instruction (Group01::Opcode opcode, Byte addressingModeFlags, Microcode::Operation operation, Byte * pRegisterAffected);
    
    void PrintInstructionSet ();
    


protected:
    static constexpr size_t memSize = 64 * 1024;
    Byte                    memory[memSize];

    Word                    SP;
    Word                    PC;
    Byte                    A;
    Byte                    X;
    Byte                    Y;

    CpuStatus               status;

protected:
    Microcode instructionSet[256];
};



