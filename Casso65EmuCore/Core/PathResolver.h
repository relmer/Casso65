#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolver
//
////////////////////////////////////////////////////////////////////////////////

class PathResolver
{
public:

    // Build the list of base directories to search (exe dir, cwd, parents)
    static std::vector<std::string> BuildSearchPaths (
        const std::string & exeDir,
        const std::string & cwd);

    // Find a file by searching base paths + relative path.
    // Returns the base path where the file was found, or "" if not found.
    static std::string FindFile (
        const std::vector<std::string> & searchPaths,
        const std::string & relativePath);

    // Get the executable's directory
    static std::string GetExecutableDirectory ();

    // Get the current working directory
    static std::string GetWorkingDirectory ();
};
