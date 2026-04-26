#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IncludeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MockFileReader
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MockFileReader : public FileReader
    {
    public:
        std::unordered_map<std::string, std::string> files;



        FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) override
        {
            FileReadResult result = {};

            auto it = files.find (filename);

            if (it != files.end ())
            {
                result.success  = true;
                result.contents = it->second;
            }
            else
            {
                result.success = false;
                result.error   = "File not found: " + filename;
            }

            return result;
        }
    };





    TEST_CLASS (IncludeDirectiveTests)
    {
    public:

        TEST_METHOD (IncludeInsertsFileContent)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["defs.a65"] = "val = $42\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet (), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"defs.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #$42 = A9 42
            Assert::AreEqual ((size_t) 2, result.bytes.size ());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }





        TEST_METHOD (DotIncludeWorks)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["defs.a65"] = "val = $55\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet (), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    .include \"defs.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x55, result.bytes[1]);
        }





        TEST_METHOD (NestedIncludesWork)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["outer.a65"] = "    include \"inner.a65\"\n";
            reader.files["inner.a65"] = "val = $33\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet (), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"outer.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x33, result.bytes[1]);
        }





        TEST_METHOD (IncludeFileNotFound_Error)
        {
            TestCpu cpu;
            MockFileReader reader;

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet (), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"missing.a65\"\n"
            );

            Assert::IsFalse (result.success, L"Should fail for missing file");
            Assert::IsTrue (result.errors.size () > 0, L"Should have error");
        }





        TEST_METHOD (NoFileReader_Error)
        {
            TestCpu cpu;

            Assembler assembler (cpu.GetInstructionSet ());
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"file.a65\"\n"
            );

            Assert::IsFalse (result.success, L"Should fail without reader");
        }
    };
}
