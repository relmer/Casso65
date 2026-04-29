#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/PathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolverTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PathResolverTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSearchPaths_ContainsExeDirAndCwd
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BuildSearchPaths_ContainsExeDirAndCwd)
    {
        auto paths = PathResolver::BuildSearchPaths ("C:/app/bin", "C:/project");

        Assert::IsTrue (paths.size () >= 2);
        Assert::AreEqual (std::string ("C:/app/bin"), paths[0]);
        Assert::AreEqual (std::string ("C:/project"), paths[1]);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSearchPaths_IncludesParentDirectories
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BuildSearchPaths_IncludesParentDirectories)
    {
        auto paths = PathResolver::BuildSearchPaths ("C:/app/x64/Debug", "C:/project/build");



        // Should include parents: C:/app/x64, C:/app, C:/project
        bool foundExeParent      = false;
        bool foundExeGrandparent = false;
        bool foundCwdParent      = false;

        for (const auto & p : paths)
        {
            if (p == "C:/app/x64")      foundExeParent      = true;
            if (p == "C:/app")          foundExeGrandparent = true;
            if (p == "C:/project")      foundCwdParent      = true;
        }

        Assert::IsTrue (foundExeParent);
        Assert::IsTrue (foundExeGrandparent);
        Assert::IsTrue (foundCwdParent);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInFirstPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInFirstPath)
    {
        // Use the repo's own machines/ directory as test data
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;  // Skip if repo root not found
        }

        std::vector<std::string> paths = { repoRoot, "C:/nonexistent" };
        std::string result = PathResolver::FindFile (paths, "machines/apple2plus.json");

        Assert::AreEqual (repoRoot, result);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInSecondPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInSecondPath)
    {
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::vector<std::string> paths = { "C:/nonexistent", repoRoot };
        std::string result = PathResolver::FindFile (paths, "machines/apple2plus.json");

        Assert::AreEqual (repoRoot, result);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_NotFound_ReturnsEmpty
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_NotFound_ReturnsEmpty)
    {
        std::vector<std::string> paths = { "C:/nonexistent1", "C:/nonexistent2" };
        std::string result = PathResolver::FindFile (paths, "nosuchfile.xyz");

        Assert::IsTrue (result.empty ());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_IndependentSearch_MachinesAndRoms
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_IndependentSearch_MachinesAndRoms)
    {
        // Verify that machines/ and roms/ can be searched independently —
        // both should resolve from the same search paths
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::vector<std::string> paths = { repoRoot };

        std::string configBase = PathResolver::FindFile (paths, "machines/apple2plus.json");
        Assert::IsFalse (configBase.empty ());

        // roms/ may or may not exist in the test environment,
        // but the search itself should not crash
        PathResolver::FindFile (paths, "roms/apple2plus.rom");
    }

private:

    // Walk up from the test DLL directory to find the repo root (has machines/)
    static std::string FindRepoRoot ()
    {
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        return PathResolver::FindFile (paths, "machines/apple2plus.json");
    }
};
