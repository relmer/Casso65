#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OptionsDialog
//
//  View -> Options... modal dialog (spec 005-disk-ii-audio FR-006).
//  Currently exposes a single toggle, "Drive Audio". The dialog is
//  built procedurally with DialogBoxIndirectParam so it carries no
//  .rc footprint.
//
////////////////////////////////////////////////////////////////////////////////

class OptionsDialog
{
public:
    // Show the dialog modally. On OK, `outDriveAudioEnabled` is set
    // to the new checkbox state and S_OK is returned. On Cancel,
    // S_FALSE is returned and the parameter is left unmodified.
    static HRESULT Show (
        HWND      hwndParent,
        HINSTANCE hInstance,
        bool      currentDriveAudioEnabled,
        bool &    outDriveAudioEnabled);
};
