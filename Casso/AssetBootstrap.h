#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssetBootstrap
//
//  First-run asset wrangling for the Casso shell:
//   * Ensure a Machines/ directory exists (extract the embedded
//     defaults if the user has none).
//   * Detect missing ROM files for a given machine config and offer
//     to download them from the AppleWin project.
//
//  Both helpers honor the existing repo layout when present (a
//  Machines/ or ROMs/ directory found via PathResolver search paths
//  is reused). Otherwise assets are placed next to casso.exe.
//
////////////////////////////////////////////////////////////////////////////////

class AssetBootstrap
{
public:

    static HRESULT  EnsureMachineConfigs  (HINSTANCE                hInstance,
                                           const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);
 
    static fs::path GetRomDirectory       (const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);
 
    static fs::path GetDiskDirectory      (const vector<fs::path> & searchPaths,
                                           const fs::path         & exeDir);
 
    static HRESULT  GetRequiredRoms       (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           vector<string>         & outRomFiles,
                                           string                 & outError);
        
    static HRESULT  HasDiskController     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           bool                   & outHasDiskController,
                                           string                 & outError);
 
    static HRESULT  CheckAndFetchRoms     (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const vector<fs::path> & searchPaths,
                                           const fs::path         & romDir,
                                           string                 & outError);

    static HRESULT  OfferBootDiskDownload (HINSTANCE                hInstance,
                                           const wstring          & machineName,
                                           HWND                     hwndParent,
                                           const fs::path         & diskDir,
                                           wstring                & outDiskPath,
                                           string                 & outError);
};
