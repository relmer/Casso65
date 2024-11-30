#pragma once



class Group00
{
protected:
    static constexpr Byte group = 0b00;

public:
    enum Opcode
    {
        BIT,        // 001 BIT
        JMP,        // 010 JMP          0100 1100 4C
        JMP_abs,    // 011 JMP (abs)    0110 1100 6C
        STY,        // 100 STY
        LDY,        // 101 LDY
        CPY,        // 110 CPY
        CPX,        // 111 CPX
    };

    static constexpr const char * instructionName[] =
    {
        "BIT",
        "JMP",
        "JMP",
        "STY",
        "LDY",
        "CPY",
        "CPX",
    };

    enum AddressingMode
    {
        __AM_First = 0,
        AM_Immediate    = 0b000,    // 010 #Immediate
        AM_ZeroPage     = 0b001,    // 001 Zero Page
        __AM_Unused_010 = 0b010,
        AM_Absolute     = 0b011,    // 011 Absolute        
        __AM_Unused_100 = 0b100,
        AM_ZeroPageX    = 0b101,    // 101 Zero Page, X
        __AM_Unused_110 = 0b110,
        AM_AbsoluteX    = 0b111,    // 111 Absolute, X
        __AM_Count
    };

    enum AddressingModeFlag
    {
        AMF_Immediate       = 0b00000001,
        AMF_ZeroPage        = 0b00000010,
        __AMF_Unused_010    = 0b00000000,
        AMF_Absolute        = 0b00001000,
        __AMF_Unused_100    = 0b00000000,
        AMF_ZeroPageX       = 0b00100000,
        __AMF_Unused_110    = 0b00000000,
        AMF_AbsoluteX       = 0b10000000,
        __AMF_AllModes      = 0b11111111 & ~(__AMF_Unused_010 | __AMF_Unused_100 | __AMF_Unused_110)
    };

    static constexpr const char * addressingModeName[] =
    {
        "#Immediate",
        "Zero Page",
        "Unused_010",
        "Absolute",
        "Unused_100",
        "Zero Page, X",
        "Unused_110",
        "Absolute, X"
    };

public:
    static Instruction CreateInstruction (Opcode opcode, AddressingMode addressingMode)
    {
        return Instruction (opcode, addressingMode, group);
    }
};
