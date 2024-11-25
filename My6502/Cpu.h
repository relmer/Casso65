#pragma once



typedef unsigned char   byte;
typedef unsigned short  word;



struct Flags
{
    byte c : 1;
    byte z : 1;
    byte i : 1;
    byte d : 1;
    byte b : 1;
    byte v : 1;
    byte n : 1;
};


union Status
{
    byte    status;
    Flags   flags;
};




enum Opcode01
{
    ORA,    // 000 = OR
    AND,    // 001 = AND
    EOR,    // 010 = XOR
    ADC,    // 011 = ADD with Carry
    STA,    // 100 = Store
    LDA,    // 101 = Load
    CMP,    // 110 = Compare
    SBC     // 111 = Subtract with Carry
};



enum AddressingMode01
{
    ZeroPageXIndirect,  // 000 (zero page, X)
    ZeroPage,           // 001 zero page
    Immediate,          // 010 #immediate
    Absolute,           // 011 absolute
    ZeroPageIndirectY,  // 100 (zero page), Y
    ZeroPageX,          // 101 zero page, X
    AbsoluteY,          // 110 absolute, Y
    AbsoluteX           // 111 absolute, X
};



struct Instruction
{
    byte Opcode         : 3;
    byte AddressingMode : 3;
    byte Group          : 2;
};



class Cpu
{
public:
    void Reset ();
    void Run ();

protected:
    word Decode (byte opcode);
    byte FetchInstruction ();

protected:
    static size_t constexpr     memSize = 64 * 1024;
    byte                        memory[memSize];

    word    pc;
    word    sp;

    Status  status;

    byte    a;
    byte    x;
    byte    y;
};

