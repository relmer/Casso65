#pragma once



#include "Instruction.h"



class Microcode
{
public:
    enum Operation
    {
        Or,
        And,
        Xor,
        AddWithCarry,
        Store,
        Load,
        Compare,
        SubtractWithCarry,
    };

public:
    Microcode () :
        isLegal (false),
        instructionName ("Illegal instruction")
    {
    }

    Microcode (Instruction instruction, const char * instructionName, bool isSingleByte, Operation operation, Byte * pRegisterAffected) :
        isLegal (true),
        instruction (instruction),
        instructionName (instructionName),
        isSingleByte (isSingleByte),
        pRegisterAffected (pRegisterAffected),
        operation (operation)
    {
    }

public:
    bool          isLegal;
    Instruction   instruction;
    const char * instructionName;
    bool          isSingleByte;
    Byte * pRegisterAffected;
    Operation     operation;
};
