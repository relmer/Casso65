#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Load_ValidJson_ParsesFields
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_ValidJson_ParsesFields)
    {
        std::string json = MinimalJson ();
        MachineConfig config;
        std::string error;



        // Use empty search paths — no ROM file to resolve
        std::vector<std::string> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr), L"Load should succeed");
        Assert::AreEqual (std::string ("TestMachine"), config.name);
        Assert::AreEqual (std::string ("6502"), config.cpu);
        Assert::AreEqual (1023000u, config.clockSpeed);
        Assert::AreEqual (size_t (1), config.memoryRegions.size ());
        Assert::AreEqual (std::string ("ram"), config.memoryRegions[0].type);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Load_RomResolvedPath_PopulatedOnSuccess
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_RomResolvedPath_PopulatedOnSuccess)
    {
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::string json = JsonWithRom ();
        MachineConfig config;
        std::string error;

        std::vector<std::string> paths = { repoRoot };
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr), L"Load should succeed with valid ROM");

        // Find the ROM region and verify resolvedPath
        bool foundRom = false;

        for (const auto & region : config.memoryRegions)
        {
            if (region.type == "rom" && !region.file.empty ())
            {
                Assert::IsFalse (region.resolvedPath.empty (),
                                 L"resolvedPath should be populated for ROM regions");
                Assert::IsTrue (region.resolvedPath.find ("roms/apple2plus.rom") != std::string::npos,
                                L"resolvedPath should contain the ROM filename");
                foundRom = true;
            }
        }

        Assert::IsTrue (foundRom, L"Should have at least one ROM region");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Load_RomNotFound_ReturnsError
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_RomNotFound_ReturnsError)
    {
        std::string json = JsonWithRom ();
        MachineConfig config;
        std::string error;



        // Search paths that won't contain any ROMs
        std::vector<std::string> paths = { "C:/nonexistent" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr), L"Load should fail when ROM not found");
        Assert::IsTrue (error.find ("ROM file not found") != std::string::npos,
                        L"Error should mention ROM not found");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Load_MissingName_ReturnsError
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_MissingName_ReturnsError)
    {
        std::string json = R"({ "cpu": "6502", "clockSpeed": 1023000 })";
        MachineConfig config;
        std::string error;

        std::vector<std::string> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (error.find ("name") != std::string::npos);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Load_InvalidCpu_ReturnsError
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_InvalidCpu_ReturnsError)
    {
        std::string json = R"({
            "name": "Test",
            "cpu": "z80",
            "clockSpeed": 1023000,
            "memory": [],
            "devices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";

        MachineConfig config;
        std::string error;

        std::vector<std::string> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (error.find ("z80") != std::string::npos);
    }

private:

    static std::string MinimalJson ()
    {
        return R"({
            "name": "TestMachine",
            "cpu": "6502",
            "clockSpeed": 1023000,
            "memory": [
                { "type": "ram", "start": "0x0000", "end": "0xBFFF" }
            ],
            "devices": [],
            "video": { "modes": ["apple2-text40"], "width": 560, "height": 384 },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string JsonWithRom ()
    {
        return R"({
            "name": "TestWithRom",
            "cpu": "6502",
            "clockSpeed": 1023000,
            "memory": [
                { "type": "ram", "start": "0x0000", "end": "0xBFFF" },
                { "type": "rom", "start": "0xD000", "end": "0xFFFF", "file": "apple2plus.rom" }
            ],
            "devices": [],
            "video": { "modes": ["apple2-text40"], "width": 560, "height": 384 },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string FindRepoRoot ()
    {
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        return PathResolver::FindFile (paths, "machines/apple2plus.json");
    }
};
