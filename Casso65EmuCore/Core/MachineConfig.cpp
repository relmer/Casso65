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
        CBRF (false, outError = format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str));
    }

    val = strtoul (pszHex, &pEnd, 16);

    CBRF (pEnd != pszHex && *pEnd == '\0', outError = format ("Invalid hex digits in address: '{}'",    str));
    CBRF (val <= 0xFFFF,                   outError = format ("Address out of range: '{}' (max $FFFF)", str));

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
    HRESULT              hr         = S_OK;
    JsonValue            root;
    JsonParseError       parseError;
    const JsonValue    * pMemArray  = nullptr;
    const JsonValue    * pDevArray  = nullptr;
    const JsonValue    * pVideo     = nullptr;
    const JsonValue    * pKeyboard  = nullptr;



    // Parse JSON
    hr = JsonParser::Parse (jsonText, root, parseError);

    if (FAILED (hr))
    {
        outError = format ("JSON parse error at line {}, column {}: {}",
                           parseError.line, parseError.column, parseError.message);
    }

    CHR (hr);

    // Required fields
    hr = root.GetString ("name", outConfig.name);
    CHRF (hr, outError = "Missing or invalid field: 'name'");

    hr = root.GetString ("cpu", outConfig.cpu);
    CHRF (hr, outError = "Missing or invalid field: 'cpu'");

    CBRF (outConfig.cpu == "6502",
          outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));

    hr = root.GetUint32 ("clockSpeed", outConfig.clockSpeed);
    CHRF (hr, outError = "Missing or invalid field: 'clockSpeed'");

    // Required sub-structures
    hr = root.GetArray ("memory", pMemArray);
    CHRF (hr, outError = "Missing required field: 'memory'");

    hr = LoadMemoryRegions (*pMemArray, searchPaths, outConfig, outError);
    CHR (hr);

    hr = root.GetArray ("devices", pDevArray);
    CHRF (hr, outError = "Missing required field: 'devices'");

    hr = LoadDevices (*pDevArray, outConfig, outError);
    CHR (hr);

    hr = root.GetObject ("video", pVideo);
    CHRF (hr, outError = "Missing required field: 'video'");
    LoadVideoConfig (*pVideo, outConfig);

    hr = root.GetObject ("keyboard", pKeyboard);
    CHRF (hr, outError = "Missing required field: 'keyboard'");
    LoadKeyboardConfig (*pKeyboard, outConfig);

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



    struct OptionalField { const char * key; string MemoryRegion::* member; };

    static const OptionalField kOptionalFields[] =
    {
        { "file",   &MemoryRegion::file   },
        { "bank",   &MemoryRegion::bank   },
        { "target", &MemoryRegion::target },
    };

    for (size_t i = 0; i < memArray.ArraySize(); i++)
    {
        const JsonValue & entry = memArray.ArrayAt (i);
        MemoryRegion      region;
        string            addrStr;
        fs::path          romRelPath;
        fs::path          found;

        CBR (entry.IsObject());

        hr = entry.GetString ("type", region.type);
        CHRF (hr, outError = format ("memory[{}]: missing 'type' field", i));

        hr = entry.GetString ("start", addrStr);
        CHRF (hr, outError = format ("memory[{}]: missing 'start' field", i));

        hr = ParseHexAddress (addrStr, region.start, outError);
        CHR (hr);

        hr = entry.GetString ("end", addrStr);
        CHRF (hr, outError = format ("memory[{}]: missing 'end' field", i));

        hr = ParseHexAddress (addrStr, region.end, outError);
        CHR (hr);

        CBRF (region.end >= region.start,
              outError = format ("memory[{}]: end (0x{:04X}) < start (0x{:04X})",
                                 i, region.end, region.start));

        for (const auto & f : kOptionalFields)
        {
            entry.GetString (f.key, region.*(f.member));
        }

        CBRF (!(region.type == "rom" && region.file.empty() && region.target.empty()),
              outError = format ("memory[{}]: ROM region requires 'file' field", i));

        if (!region.file.empty())
        {
            romRelPath = fs::path ("roms") / region.file;
            found      = PathResolver::FindFile (searchPaths, romRelPath);

            CBRF (!found.empty(),
                  outError = format ("ROM file not found: roms/{}. "
                                    "Run scripts/FetchRoms.ps1 to download ROM images.",
                                    region.file));

            region.resolvedPath = found.string();
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



    for (size_t i = 0; i < devArray.ArraySize(); i++)
    {
        const JsonValue & entry = devArray.ArrayAt (i);
        DeviceConfig      device;
        string            addrStr;

        CBR (entry.IsObject());

        hr = entry.GetString ("type", device.type);
        CHRF (hr, outError = format ("devices[{}]: missing 'type' field", i));

        hr = entry.GetString ("address", addrStr);

        if (SUCCEEDED (hr))
        {
            hr = ParseHexAddress (addrStr, device.address, outError);
            CHR (hr);

            device.start      = device.address;
            device.end        = device.address;
            device.hasAddress = true;
        }

        hr = entry.GetString ("start", addrStr);

        if (SUCCEEDED (hr))
        {
            hr = ParseHexAddress (addrStr, device.start, outError);
            CHR (hr);

            hr = entry.GetString ("end", addrStr);
            CHRF (hr, outError = format ("devices[{}]: missing 'end' field", i));

            hr = ParseHexAddress (addrStr, device.end, outError);
            CHR (hr);

            device.hasRange = true;
        }

        hr = entry.GetInt ("slot", device.slot);

        if (SUCCEEDED (hr))
        {
            device.hasSlot = true;

            CBRF (device.slot >= 1 && device.slot <= 7,
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
    HRESULT           hr     = S_OK;
    const JsonValue * pModes = nullptr;



    hr = video.GetArray ("modes", pModes);

    if (SUCCEEDED (hr))
    {
        for (size_t i = 0; i < pModes->ArraySize (); i++)
        {
            if (pModes->ArrayAt (i).IsString ())
            {
                outConfig.videoConfig.modes.push_back (pModes->ArrayAt (i).GetString ());
            }
        }
    }

    video.GetInt ("width", outConfig.videoConfig.width);
    video.GetInt ("height", outConfig.videoConfig.height);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadKeyboardConfig
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::LoadKeyboardConfig (const JsonValue & keyboard, MachineConfig & outConfig)
{
    keyboard.GetString ("type", outConfig.keyboardType);
}




