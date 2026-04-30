#include "Pch.h"

#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> PathResolver::BuildSearchPaths (
    const std::string & exeDir,
    const std::string & cwd)
{
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

    return searchBases;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindFile
//
////////////////////////////////////////////////////////////////////////////////

std::string PathResolver::FindFile (
    const std::vector<std::string> & searchPaths,
    const std::string & relativePath)
{
    for (const auto & base : searchPaths)
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
//  GetExecutableDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::string PathResolver::GetExecutableDirectory ()
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
//  GetWorkingDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::string PathResolver::GetWorkingDirectory ()
{
    char path[MAX_PATH] = {};
    GetCurrentDirectoryA (MAX_PATH, path);
    return std::string (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WideToNarrow
//
////////////////////////////////////////////////////////////////////////////////

std::string PathResolver::WideToNarrow (const std::wstring & wide)
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
//  NarrowToWide
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PathResolver::NarrowToWide (const std::string & narrow)
{
    if (narrow.empty ())
    {
        return L"";
    }

    int len = MultiByteToWideChar (CP_UTF8, 0, narrow.c_str (), -1, NULL, 0);
    std::wstring wide (len - 1, L'\0');
    MultiByteToWideChar (CP_UTF8, 0, narrow.c_str (), -1, wide.data (), len);
    return wide;
}
