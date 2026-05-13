#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSettings
//
//  Per-machine remembered disk-mount paths in HKCU.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSettings
{
public:

    static HRESULT  ReadSavedDiskPath  (int             drive,
                                        const wstring & machineName,
                                        wstring       & outPath);

    static HRESULT  WriteSavedDiskPath (int             drive,
                                        const wstring & machineName,
                                        const wstring & path);
};
