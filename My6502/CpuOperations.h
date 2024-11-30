#pragma once

#include "Cpu.h"



class CpuOperations
{
public:
    CpuOperations () = delete;

    static void AddWithCarry      (Cpu & cpu, Byte operand);
    static void And               (Cpu & cpu, Byte operand);
    static void BitTest           (Cpu & cpu, Byte operand);
    static void Compare           (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Decrement         (Cpu & cpu, Word effectiveAddress);
    static void Increment         (Cpu & cpu, Word effectiveAddress);
    static void Load              (Cpu & cpu, Byte & registerAffected, Byte operand);
    static void Jump              (Cpu & cpu, Instruction instruction, Word operand);
    static void Or                (Cpu & cpu, Byte operand);
    static void RotateLeft        (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void RotateRight       (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void ShiftLeft         (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void ShiftRight        (Cpu & cpu, Byte * registerAffected, Word effectiveAddress);
    static void Store             (Cpu & cpu, Byte & registerAffected, Word effectiveAddress);
    static void SubtractWithCarry (Cpu & cpu, Byte operand);
    static void Xor               (Cpu & cpu, Byte operand);
};
