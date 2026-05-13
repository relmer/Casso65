#include "Pch.h"

#include "DiskSettings.h"
#include "Core/PathResolver.h"
#include "Ehm.h"
#include "RegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Storage layout
//
//  HKCU\Software\relmer\Casso\Machines\<machine> stores the per-machine
//  drive-bay state. Disk paths are stored relative to casso.exe when
//  the disk lives under or beside the exe (the common case for the
//  default Disks/ peer dir). User-mounted disks from elsewhere keep
//  their absolute path. Result: the casso.exe + Disks/ tree stays
//  portable across moves while explicitly-located disks are remembered
//  exactly as the user pointed at them.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr LPCWSTR  s_kpszMachinesRoot       = L"Machines";
static constexpr LPCWSTR  s_kpszDiskValueNames[2]  = { L"Disk1", L"Disk2" };





////////////////////////////////////////////////////////////////////////////////
//
//  MakeMachineSubkey
//
////////////////////////////////////////////////////////////////////////////////

static wstring MakeMachineSubkey (const wstring & machineName)
{
    wstring  sub = s_kpszMachinesRoot;


    if (!machineName.empty())
    {
        sub += L"\\";
        sub += machineName;
    }

    return sub;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeLegacyDiskValueName
//
//  Pre-hierarchy value name (e.g. Disk1.apple2e). Kept for one-shot
//  migration of users who already have a saved disk under the flat
//  layout from the previous build -- read from the legacy slot if the
//  new slot is empty, then re-write into the new slot.
//
////////////////////////////////////////////////////////////////////////////////

static wstring MakeLegacyDiskValueName (int drive, const wstring & machineName)
{
    wstring  name;


    name = (drive == 0) ? L"Disk1." : L"Disk2.";
    name += machineName;
    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRegistryDiskPath
//
//  Convert a (possibly absolute) path into the form that should land
//  in the registry: relative to the exe directory when the disk lives
//  under it, otherwise the original absolute path.
//
////////////////////////////////////////////////////////////////////////////////

static wstring MakeRegistryDiskPath (const wstring & inputPath)
{
    fs::path     input  = fs::path (inputPath);
    fs::path     exeDir;
    fs::path     rel;
    error_code   ec;
    wstring      first;


    if (input.empty())
    {
        return wstring();
    }

    if (input.is_relative())
    {
        return inputPath;
    }

    exeDir = PathResolver::GetExecutableDirectory();
    rel    = fs::relative (input, exeDir, ec);

    if (ec || rel.empty())
    {
        return inputPath;
    }

    // If `relative` produced a path that escapes the exe directory
    // (starts with `..`), fall back to the absolute path so we don't
    // bake a brittle climb-out into the registry.
    first = rel.begin() == rel.end() ? wstring() : rel.begin()->wstring();

    if (first == L"..")
    {
        return inputPath;
    }

    return rel.wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveRegistryDiskPath
//
//  Inverse of MakeRegistryDiskPath: relative entries are joined with
//  the exe directory; absolute entries pass through unchanged.
//
////////////////////////////////////////////////////////////////////////////////

static wstring ResolveRegistryDiskPath (const wstring & storedValue)
{
    fs::path  stored = fs::path (storedValue);


    if (storedValue.empty() || stored.is_absolute())
    {
        return storedValue;
    }

    return (PathResolver::GetExecutableDirectory() / stored).lexically_normal().wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSavedDiskPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::ReadSavedDiskPath (
    int             drive,
    const wstring & machineName,
    wstring       & outPath)
{
    HRESULT  hr     = S_OK;
    wstring  subkey = MakeMachineSubkey (machineName);
    wstring  raw;

    

    outPath.clear();

    hr = RegistrySettings::ReadString (subkey.c_str(), s_kpszDiskValueNames[drive], raw);

    if (hr == S_FALSE || (hr == S_OK && raw.empty()))
    {
        // Fall back to the legacy flat-namespace value from the prior
        // build. If found, copy it forward to the hierarchical layout
        // (now in relative-path form) so subsequent reads hit the new
        // location.
        wstring  legacy;
        HRESULT  hrLegacy = RegistrySettings::ReadString (MakeLegacyDiskValueName (drive, machineName).c_str(), legacy);

        if (hrLegacy == S_OK && !legacy.empty())
        {
            raw = legacy;
            HRESULT hrMigrate = RegistrySettings::WriteString (subkey.c_str(), s_kpszDiskValueNames[drive],
                                                               MakeRegistryDiskPath (legacy));
            IGNORE_RETURN_VALUE (hrMigrate, S_OK);
        }
    }

    if (!raw.empty())
    {
        outPath = ResolveRegistryDiskPath (raw);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSavedDiskPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSettings::WriteSavedDiskPath (
    int             drive,
    const wstring & machineName,
    const wstring & path)
{
    wstring  subkey  = MakeMachineSubkey (machineName);
    wstring  toStore = path.empty() ? wstring() : MakeRegistryDiskPath (path);


    return RegistrySettings::WriteString (subkey.c_str(), s_kpszDiskValueNames[drive], toStore);
}
