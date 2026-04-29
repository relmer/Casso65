#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Ehm.h"
#include "Shell/EmulatorShell.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  GetExecutableDirectory
//
////////////////////////////////////////////////////////////////////////////////

static std::string GetExecutableDirectory ()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA (nullptr, path, MAX_PATH);

    std::string dir (path);
    size_t pos = dir.find_last_of ("\\/");

    if (pos != std::string::npos)
    {
        dir = dir.substr (0, pos);
    }

    return dir;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentDirectory
//
////////////////////////////////////////////////////////////////////////////////

static std::string GetWorkingDirectory ()
{
    char path[MAX_PATH] = {};
    GetCurrentDirectoryA (MAX_PATH, path);
    return std::string (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindFileInSearchPaths — searches exe dir, cwd, and subdirectories of each
//
////////////////////////////////////////////////////////////////////////////////

static std::string FindFileInSearchPaths (const std::string & relativePath)
{
    std::string exeDir = GetExecutableDirectory ();
    std::string cwd    = GetWorkingDirectory ();

    // Search order: exe dir, cwd, then each with common parent patterns
    std::vector<std::string> searchBases = { exeDir, cwd };

    // Also try parent directories (handles running from x64/Debug/)
    for (const auto & base : { exeDir, cwd })
    {
        size_t pos = base.find_last_of ("\\/");

        if (pos != std::string::npos)
        {
            std::string parent = base.substr (0, pos);
            searchBases.push_back (parent);

            size_t pos2 = parent.find_last_of ("\\/");

            if (pos2 != std::string::npos)
            {
                searchBases.push_back (parent.substr (0, pos2));
            }
        }
    }

    for (const auto & base : searchBases)
    {
        std::string candidate = base + "/" + relativePath;
        std::ifstream test (candidate);

        if (test.good ())
        {
            return base;
        }
    }

    return "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseCommandLine (
    LPWSTR lpCmdLine,
    std::wstring & outMachine,
    std::wstring & outDisk1,
    std::wstring & outDisk2)
{
    HRESULT hr = S_OK;

    int argc = 0;
    LPWSTR * argv = CommandLineToArgvW (lpCmdLine, &argc);

    CPRA (argv);

    for (int i = 0; i < argc; i++)
    {
        std::wstring arg (argv[i]);

        if (arg == L"--machine" && i + 1 < argc)
        {
            outMachine = argv[++i];
        }
        else if (arg == L"--disk1" && i + 1 < argc)
        {
            outDisk1 = argv[++i];
        }
        else if (arg == L"--disk2" && i + 1 < argc)
        {
            outDisk2 = argv[++i];
        }
    }

    LocalFree (argv);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WideToNarrow — convert std::wstring to std::string (UTF-8)
//
////////////////////////////////////////////////////////////////////////////////

static std::string WideToNarrow (const std::wstring & wide)
{
    if (wide.empty ())
    {
        return "";
    }

    int len = WideCharToMultiByte (CP_UTF8, 0, wide.c_str (), -1, NULL, 0, NULL, NULL);
    std::string narrow (len - 1, '\0');
    WideCharToMultiByte (CP_UTF8, 0, wide.c_str (), -1, narrow.data (), len, NULL, NULL);
    return narrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  wWinMain
//
////////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    HRESULT hr = S_OK;

    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (nCmdShow);

    std::wstring machineName;
    std::wstring disk1Path;
    std::wstring disk2Path;
    std::string  error;



    // Register GUI error notification so EHM errors show a MessageBox
    SetNotifyFunction ([] (const wchar_t * message)
    {
        MessageBoxW (NULL, message, L"Casso65 Emulator", MB_OK | MB_ICONERROR);
    });

    // Initialize COM for WASAPI
    hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    CHRA (hr);

    // Parse command line
    hr = ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path);
    CHR (hr);

    // Default machine if not specified
    if (machineName.empty ())
    {
        machineName = L"apple2plus";
    }

    {
        // Find machine config using search paths
        std::string narrowMachine = WideToNarrow (machineName);
        std::string configRelPath = "machines/" + narrowMachine + ".json";
        std::string basePath      = FindFileInSearchPaths (configRelPath);

        CBRN (!basePath.empty (),
              std::format (L"Unknown machine '{}'. Config file not found.\n"
                           L"Searched for '{}' in exe directory, current directory, and parent directories.",
                           machineName,
                           std::wstring (configRelPath.begin (), configRelPath.end ())).c_str ());

        std::string configPath = basePath + "/" + configRelPath;

        // Load config file
        std::ifstream configFile (configPath);
        bool configGood = configFile.good ();
        CBRN (configGood,
              std::format (L"Cannot open machine config:\n{}",
                           std::wstring (configPath.begin (), configPath.end ())).c_str ());

        std::stringstream ss;
        ss << configFile.rdbuf ();
        std::string jsonText = ss.str ();

        // Parse config
        MachineConfig config;
        hr = MachineConfigLoader::Load (jsonText, basePath, config, error);
        CHRN (hr, std::format (L"Failed to load machine config:\n{}",
                               std::wstring (error.begin (), error.end ())).c_str ());

        // Validate disk images
        if (!disk1Path.empty ())
        {
            std::ifstream disk (WideToNarrow (disk1Path), std::ios::binary | std::ios::ate);
            bool diskGood = disk.good ();
            CBRN (diskGood,
                  std::format (L"Disk image not found:\n{}", disk1Path).c_str ());

            auto size = disk.tellg ();
            bool validSize = (size == 143360);
            CBRN (validSize,
                  std::format (L"Disk image '{}' is not a valid .dsk file\n(expected 143360 bytes, got {} bytes)",
                               disk1Path, static_cast<int64_t> (size)).c_str ());
        }

        // Initialize emulator
        EmulatorShell shell;
        hr = shell.Initialize (hInstance, config, basePath,
                               WideToNarrow (disk1Path), WideToNarrow (disk2Path));
        CHRN (hr, L"Failed to initialize emulator");

        // Run message loop
        return shell.RunMessageLoop ();
    }

Error:
    CoUninitialize ();
    return FAILED (hr) ? 1 : 0;
}
