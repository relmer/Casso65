#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace StructTests
{
    TEST_CLASS (StructDefinitionTests)
    {
    public:

        TEST_METHOD (StructDefinesMemberOffsets)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct PlayerState\n"
                "xpos ds 1\n"
                "ypos ds 1\n"
                "health ds 2\n"
                "    end struct\n"
                "    lda #PlayerState.xpos\n"
                "    ldx #PlayerState.ypos\n"
                "    ldy #PlayerState.health\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #0 (xpos offset), LDX #1 (ypos), LDY #2 (health)
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xA2, result.bytes[2]);
            Assert::AreEqual ((Byte) 0x01, result.bytes[3]);
            Assert::AreEqual ((Byte) 0xA0, result.bytes[4]);
            Assert::AreEqual ((Byte) 0x02, result.bytes[5]);
        }





        TEST_METHOD (StructSizeAsSymbol)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Obj\n"
                "x ds 1\n"
                "y ds 1\n"
                "flags ds 1\n"
                "    end struct\n"
                "    lda #Obj\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #3 (struct size)
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x03, result.bytes[1]);
        }





        TEST_METHOD (StructWithStartOffset)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Regs, 4\n"
                "regA ds 1\n"
                "regB ds 1\n"
                "    end struct\n"
                "    lda #Regs.regA\n"
                "    ldx #Regs.regB\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #4 (offset 4), LDX #5 (offset 5)
            Assert::AreEqual ((Byte) 0x04, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x05, result.bytes[3]);
        }





        TEST_METHOD (StructWithDbDwMembers)
        {
            TestCpu cpu;

            auto result = cpu.Assemble (
                "    .org $1000\n"
                "    struct Pkt\n"
                "id db\n"
                "len dw\n"
                "    end struct\n"
                "    lda #Pkt.id\n"
                "    ldx #Pkt.len\n"
                "    ldy #Pkt\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x00, result.bytes[1]);  // id = offset 0
            Assert::AreEqual ((Byte) 0x01, result.bytes[3]);  // len = offset 1
            Assert::AreEqual ((Byte) 0x03, result.bytes[5]);  // size = 3
        }
    };
}
