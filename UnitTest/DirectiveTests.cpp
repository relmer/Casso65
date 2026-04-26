#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DirectiveTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler (AssemblerOptions opts = {})
    {
        TestCpu cpu;
        cpu.InitForTest ();
        return Assembler (cpu.GetInstructionSet (), opts);
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SynonymTests — AS65 directive synonyms without leading dot
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SynonymTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Db_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Db_EmitsByte)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db $42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x42, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Byt_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Byt_EmitsByte)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("byt $FF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xFF, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Byte_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Byte_EmitsByte)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("byte $AB");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xAB, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcb_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcb_EmitsByte)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("fcb $10, $20");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x10, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x20, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcc_EmitsString
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcc_EmitsString)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("fcc \"AB\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x41, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dw_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dw_EmitsWord)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("dw $1234");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x34, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x12, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Word_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Word_EmitsWord)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("word $ABCD");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xCD, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xAB, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcw_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcw_EmitsWord)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("fcw $BEEF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEF, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xBE, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fdb_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fdb_EmitsWord)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("fdb $CAFE");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xFE, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xCA, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Org_SetsOrigin
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Org_SetsOrigin)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("org $8000\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x8000, r.startAddress);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DsDirectiveTests — .DS / ds / dsb / rmb storage reserve
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DsDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_ReservesZeroFilledBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_ReservesZeroFilledBytes)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("ds 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size ());

            for (size_t i = 0; i < 4; i++)
            {
                Assert::AreEqual ((Byte) 0x00, r.bytes[i]);
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_WithFillValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_WithFillValue)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("ds 3, $AA");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 3, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xAA, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dsb_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dsb_Synonym)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("dsb 2, $FF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xFF, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Rmb_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Rmb_Synonym)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("rmb 5");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotDs_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotDs_Synonym)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble (".ds 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_AdvancesPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_AdvancesPC)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble (".org $100\nds 4\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[4]);  // NOP at offset 4
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DdDirectiveTests — .DD / dd double-word
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DdDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dd_EmitsLittleEndian32
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dd_EmitsLittleEndian32)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("dd $12345678");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x78, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x56, r.bytes[1]);
            Assert::AreEqual ((Byte) 0x34, r.bytes[2]);
            Assert::AreEqual ((Byte) 0x12, r.bytes[3]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotDd_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotDd_Synonym)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble (".dd $AABBCCDD");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xDD, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xBB, r.bytes[2]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[3]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dd_MultipleValues
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dd_MultipleValues)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("dd $01, $02");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 8, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x01, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[1]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[2]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[3]);
            Assert::AreEqual ((Byte) 0x02, r.bytes[4]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EndDirectiveTests — .END / end
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EndDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  End_StopsAssembly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (End_StopsAssembly)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("NOP\nend\nLDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotEnd_StopsAssembly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotEnd_StopsAssembly)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("NOP\n.end\nLDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  End_WithExpression_Accepted
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (End_WithExpression_Accepted)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble (".org $1000\nNOP\nend $1000");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AlignDirectiveTests — .ALIGN / align
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AlignDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_PadsToAlignment
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_PadsToAlignment)
        {
            Assembler a = BuildAssembler ();
            // 1 byte NOP at address 0, then align 4 → should pad 3 bytes
            auto r = a.Assemble ("NOP\nalign 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_NoArg_PadsToEven
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_NoArg_PadsToEven)
        {
            Assembler a = BuildAssembler ();
            // 1 byte NOP at address 0 (odd PC=1), align → pad 1 byte to reach 2
            auto r = a.Assemble ("NOP\nalign");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_AlreadyAligned_NoPad
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_AlreadyAligned_NoPad)
        {
            Assembler a = BuildAssembler ();
            // 2 NOPs → PC=2, align to 2 → no padding needed
            auto r = a.Assemble ("NOP\nNOP\nalign 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotAlign_Works
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotAlign_Works)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("NOP\n.align 4\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[4]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_UsesFillByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_UsesFillByte)
        {
            AssemblerOptions opts = {};
            opts.fillByte = 0xCC;
            Assembler a = BuildAssembler (opts);

            auto r = a.Assemble ("NOP\nalign 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xCC, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[2]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[3]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ErrorDirectiveTests — .ERROR / error
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ErrorDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_CausesFailure
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_CausesFailure)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("error \"Something went wrong\"");

            Assert::IsFalse (r.success);
            Assert::AreEqual ((size_t) 1, r.errors.size ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_MessagePreserved
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_MessagePreserved)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("error \"Custom message\"");

            Assert::IsFalse (r.success);
            Assert::IsTrue (r.errors[0].message.find ("Custom message") != std::string::npos);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotError_Works
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotError_Works)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble (".error \"fail\"");

            Assert::IsFalse (r.success);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_SkippedInFalseConditional
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_SkippedInFalseConditional)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("if 0\nerror \"should not fire\"\nendif\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EscapeSequenceTests — backslash escapes in .BYTE / db strings
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EscapeSequenceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Newline_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Newline_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\n\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x0A, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Tab_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Tab_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\t\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x09, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CarriageReturn_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CarriageReturn_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\r\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x0D, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Backslash_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Backslash_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\\\\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x5C, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Bell_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Bell_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\a\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x07, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Backspace_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Backspace_Escape)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"\\b\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x08, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MultipleEscapes_InString
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MultipleEscapes_InString)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("db \"A\\nB\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 3, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x41, r.bytes[0]);  // 'A'
            Assert::AreEqual ((Byte) 0x0A, r.bytes[1]);  // \n
            Assert::AreEqual ((Byte) 0x42, r.bytes[2]);  // 'B'
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SegmentNoopTests — code/data/bss as no-ops
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SegmentNoopTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Code_IsNoop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Code_IsNoop)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("code\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Data_IsNoop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Data_IsNoop)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("data\ndb $42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size ());
            Assert::AreEqual ((Byte) 0x42, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Bss_IsNoop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Bss_IsNoop)
        {
            Assembler a = BuildAssembler ();
            auto r = a.Assemble ("bss\nds 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size ());
        }
    };
}
