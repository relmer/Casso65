#include "Pch.h"

#include "MachineConfig.h"
#include "JsonParser.h"
#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParseHexAddress
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::ParseHexAddress (const string & str, Word & outAddr, string & outError)
{
    HRESULT       hr     = S_OK;
    LPCSTR        pszHex = str.c_str();
    char        * pEnd   = nullptr;
    unsigned long val    = 0;



    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        pszHex += 2;
    }
    else if (str.size() >= 1 && str[0] == '$')
    {
        pszHex++;
    }
    else
    {
        CBR_SetError (false, outError = format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str));
    }

    val = strtoul (pszHex, &pEnd, 16);

    CBR_SetError (pEnd != pszHex && *pEnd == '\0', outError = format ("Invalid hex digits in address: '{}'",    str));
    CBR_SetError (val <= 0xFFFF,                   outError = format ("Address out of range: '{}' (max $FFFF)", str));

    outAddr = static_cast<Word> (val);


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::Load (
    const string           & jsonText,
    const vector<fs::path> & searchPaths,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT        hr         = S_OK;
    JsonValue      root;
    JsonParseError parseError;



    // Parse JSON
    hr = JsonParser::Parse (jsonText, root, parseError);

    CBR_SetError (SUCCEEDED (hr),
                  outError = format ("JSON parse error at line {}, column {}: {}",
                                    parseError.line, parseError.column,
                                    parseError.message));

    CBR (root.IsObject ());

    // Required: name
    CBR_SetError (root.HasKey ("name") && root.Get ("name").IsString (),
                  outError = "Missing required field: 'name'");
    outConfig.name = root.Get ("name").GetString ();

    // Required: cpu
    CBR_SetError (root.HasKey ("cpu") && root.Get ("cpu").IsString (),
                  outError = "Missing required field: 'cpu'");
    outConfig.cpu = root.Get ("cpu").GetString ();

    CBR_SetError (outConfig.cpu == "6502",
                  outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));

    // Required: clockSpeed
    CBR_SetError (root.HasKey ("clockSpeed") && root.Get ("clockSpeed").IsNumber (),
                  outError = "Missing required field: 'clockSpeed'");
    outConfig.clockSpeed = static_cast<uint32_t> (root.Get ("clockSpeed").GetNumber ());

    // Required: memory array
    CBR_SetError (root.HasKey ("memory") && root.Get ("memory").IsArray (),
                  outError = "Missing required field: 'memory'");

    hr = LoadMemoryRegions (root.Get ("memory"), searchPaths, outConfig, outError);
    CHR (hr);

    // Required: devices array
    CBR_SetError (root.HasKey ("devices") && root.Get ("devices").IsArray (),
                  outError = "Missing required field: 'devices'");

    hr = LoadDevices (root.Get ("devices"), outConfig, outError);
    CHR (hr);

    // Required: video object
    CBR_SetError (root.HasKey ("video") && root.Get ("video").IsObject (),
                  outError = "Missing required field: 'video'");
    LoadVideoConfig (root.Get ("video"), outConfig);

    // Required: keyboard object
    CBR_SetError (root.HasKey ("keyboard") && root.Get ("keyboard").IsObject (),
                  outError = "Missing required field: 'keyboard'");
    LoadKeyboardConfig (root.Get ("keyboard"), outConfig);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMemoryRegions
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadMemoryRegions (
    const JsonValue        & memArray,
    const vector<fs::path> & searchPaths,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT hr = S_OK;



    for (size_t i = 0; i < memArray.ArraySize (); i++)
    {
        const JsonValue & entry = memArray.ArrayAt (i);
        MemoryRegion      region;
        fs::path          romRelPath;
        fs::path          found;

        CBR (entry.IsObject ());

        CBR_SetError (entry.HasKey ("type") && entry.Get ("type").IsString (),
                      outError = format ("memory[{}]: missing 'type' field", i));
        region.type = entry.Get ("type").GetString ();

        CBR_SetError (entry.HasKey ("start") && entry.Get ("start").IsString (),
                      outError = format ("memory[{}]: missing 'start' field", i));
        hr = ParseHexAddress (entry.Get ("start").GetString (), region.start, outError);
        CHR (hr);

        CBR_SetError (entry.HasKey ("end") && entry.Get ("end").IsString (),
                      outError = format ("memory[{}]: missing 'end' field", i));
        hr = ParseHexAddress (entry.Get ("end").GetString (), region.end, outError);
        CHR (hr);

        CBR_SetError (region.end >= region.start,
                      outError = format ("memory[{}]: end (0x{:04X}) < start (0x{:04X})",
                                         i, region.end, region.start));

        if (entry.HasKey ("file") && entry.Get ("file").IsString ())
        {
            region.file = entry.Get ("file").GetString ();
        }

        if (entry.HasKey ("bank") && entry.Get ("bank").IsString ())
        {
            region.bank = entry.Get ("bank").GetString ();
        }

        if (entry.HasKey ("target") && entry.Get ("target").IsString ())
        {
            region.target = entry.Get ("target").GetString ();
        }

        CBR_SetError (!(region.type == "rom" && region.file.empty () && region.target.empty ()),
                      outError = format ("memory[{}]: ROM region requires 'file' field", i));

        if (!region.file.empty ())
        {
            romRelPath = fs::path ("roms") / region.file;
            found      = PathResolver::FindFile (searchPaths, romRelPath);

            CBR_SetError (!found.empty (),
                          outError = format ("ROM file not found: roms/{}. "
                                            "Run scripts/FetchRoms.ps1 to download ROM images.",
                                            region.file));

            region.resolvedPath = found.string ();
        }

        outConfig.memoryRegions.push_back (region);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadDevices (
    const JsonValue & devArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT hr = S_OK;



    for (size_t i = 0; i < devArray.ArraySize (); i++)
    {
        const JsonValue & entry = devArray.ArrayAt (i);
        DeviceConfig      device;

        CBR (entry.IsObject ());

        CBR_SetError (entry.HasKey ("type") && entry.Get ("type").IsString (),
                      outError = format ("devices[{}]: missing 'type' field", i));
        device.type = entry.Get ("type").GetString ();

        if (entry.HasKey ("address") && entry.Get ("address").IsString ())
        {
            hr = ParseHexAddress (entry.Get ("address").GetString (),
                                  device.address, outError);
            CHR (hr);

            device.start      = device.address;
            device.end        = device.address;
            device.hasAddress = true;
        }

        if (entry.HasKey ("start") && entry.Get ("start").IsString () &&
            entry.HasKey ("end")   && entry.Get ("end").IsString ())
        {
            hr = ParseHexAddress (entry.Get ("start").GetString (),
                                  device.start, outError);
            CHR (hr);

            hr = ParseHexAddress (entry.Get ("end").GetString (),
                                  device.end, outError);
            CHR (hr);

            device.hasRange = true;
        }

        if (entry.HasKey ("slot") && entry.Get ("slot").IsNumber ())
        {
            device.slot    = entry.Get ("slot").GetInt ();
            device.hasSlot = true;

            CBR_SetError (device.slot >= 1 && device.slot <= 7,
                          outError = format ("devices[{}]: slot must be 1-7, got {}",
                                            i, device.slot));

            device.start = static_cast<Word> (0xC080 + device.slot * 16);
            device.end   = static_cast<Word> (0xC08F + device.slot * 16);
        }

        outConfig.devices.push_back (device);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadVideoConfig
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::LoadVideoConfig (const JsonValue & video, MachineConfig & outConfig)
{
    if (video.HasKey ("modes") && video.Get ("modes").IsArray ())
    {
        const JsonValue & modes = video.Get ("modes");

        for (size_t i = 0; i < modes.ArraySize (); i++)
        {
            if (modes.ArrayAt (i).IsString ())
            {
                outConfig.videoConfig.modes.push_back (modes.ArrayAt (i).GetString ());
            }
        }
    }

    if (video.HasKey ("width") && video.Get ("width").IsNumber ())
    {
        outConfig.videoConfig.width = video.Get ("width").GetInt ();
    }

    if (video.HasKey ("height") && video.Get ("height").IsNumber ())
    {
        outConfig.videoConfig.height = video.Get ("height").GetInt ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadKeyboardConfig
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::LoadKeyboardConfig (const JsonValue & keyboard, MachineConfig & outConfig)
{
    if (keyboard.HasKey ("type") && keyboard.Get ("type").IsString ())
    {
        outConfig.keyboardType = keyboard.Get ("type").GetString ();
    }
}




