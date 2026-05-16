#include "Pch.h"

#include "OptionsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope state
//
////////////////////////////////////////////////////////////////////////////////

// IDs are local to the dialog template; no resource.h entries required.
static constexpr WORD  s_kIdcDriveAudioCheck = 1001;
static constexpr WORD  s_kIdcOkButton        = IDOK;
static constexpr WORD  s_kIdcCancelButton    = IDCANCEL;

struct OptionsDialogState
{
    bool   driveAudioEnabled;
    bool   accepted;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppendWide
//
//  Helper that writes a NUL-terminated UTF-16 string into a contiguous
//  byte buffer at a 2-byte-aligned offset. Used by BuildTemplate to
//  emit the variable-length text fields inside DLGTEMPLATEEX /
//  DLGITEMTEMPLATEEX.
//
////////////////////////////////////////////////////////////////////////////////

static void AppendWide (vector<BYTE> & buf, const wchar_t * text)
{
    size_t  i   = 0;
    size_t  len = 0;

    if (text == nullptr)
    {
        buf.push_back (0);
        buf.push_back (0);
        return;
    }

    len = wcslen (text);

    for (i = 0; i <= len; i++)
    {
        WORD  ch = static_cast<WORD> (text[i]);

        buf.push_back (static_cast<BYTE> (ch & 0xFF));
        buf.push_back (static_cast<BYTE> ((ch >> 8) & 0xFF));
    }
}


static void AlignDword (vector<BYTE> & buf)
{
    while ((buf.size() & 0x3) != 0)
    {
        buf.push_back (0);
    }
}


static void AppendItem (
    vector<BYTE> &  buf,
    DWORD           style,
    DWORD           exStyle,
    short           x,
    short           y,
    short           cx,
    short           cy,
    WORD            id,
    const wchar_t * windowClass,
    const wchar_t * title)
{
    DLGITEMTEMPLATE  item = {};

    AlignDword (buf);

    item.style           = style;
    item.dwExtendedStyle = exStyle;
    item.x               = x;
    item.y               = y;
    item.cx              = cx;
    item.cy              = cy;
    item.id              = id;

    for (size_t i = 0; i < sizeof (item); i++)
    {
        buf.push_back (reinterpret_cast<const BYTE *> (&item)[i]);
    }

    // Window class -- use the "atom-style" 0xFFFF prefix for the
    // standard controls.
    if (windowClass != nullptr)
    {
        WORD  marker = 0xFFFF;
        WORD  atom   = 0;

        if (wcscmp (windowClass, L"BUTTON") == 0)
        {
            atom = 0x0080;
        }
        else
        {
            // Unsupported class; emit as a string instead.
            AppendWide (buf, windowClass);
            atom = 0;
        }

        if (atom != 0)
        {
            buf.push_back (static_cast<BYTE> (marker & 0xFF));
            buf.push_back (static_cast<BYTE> ((marker >> 8) & 0xFF));
            buf.push_back (static_cast<BYTE> (atom & 0xFF));
            buf.push_back (static_cast<BYTE> ((atom >> 8) & 0xFF));
        }
    }
    else
    {
        buf.push_back (0);
        buf.push_back (0);
    }

    AppendWide (buf, title);

    // Creation data length (0 = no creation data).
    buf.push_back (0);
    buf.push_back (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildTemplate
//
//  Constructs an in-memory DLGTEMPLATE describing the Options dialog.
//  Three controls: a "Drive Audio" check box, an OK button, a
//  Cancel button.
//
////////////////////////////////////////////////////////////////////////////////

static void BuildTemplate (vector<BYTE> & buf)
{
    DLGTEMPLATE      tmpl = {};

    tmpl.style           = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION |
                           DS_MODALFRAME | DS_CENTER | DS_SETFONT;
    tmpl.dwExtendedStyle = 0;
    tmpl.cdit            = 3;
    tmpl.x               = 0;
    tmpl.y               = 0;
    tmpl.cx              = 180;
    tmpl.cy              = 90;

    buf.clear();

    for (size_t i = 0; i < sizeof (tmpl); i++)
    {
        buf.push_back (reinterpret_cast<const BYTE *> (&tmpl)[i]);
    }

    // Menu, class, title.
    AppendWide (buf, nullptr);
    AppendWide (buf, nullptr);
    AppendWide (buf, L"Options");

    // Font (required by DS_SETFONT): point size + face name.
    buf.push_back (8);
    buf.push_back (0);
    AppendWide (buf, L"MS Shell Dlg");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                0, 14, 18, 150, 14,
                s_kIdcDriveAudioCheck,
                L"BUTTON",
                L"&Drive Audio");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                0, 36, 64, 50, 16,
                s_kIdcOkButton,
                L"BUTTON",
                L"OK");

    AppendItem (buf,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 94, 64, 50, 16,
                s_kIdcCancelButton,
                L"BUTTON",
                L"Cancel");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialogProc
//
////////////////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK DialogProc (
    HWND   hwnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    OptionsDialogState *  state = nullptr;

    switch (uMsg)
    {
        case WM_INITDIALOG:
            state = reinterpret_cast<OptionsDialogState *> (lParam);
            SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (state));

            CheckDlgButton (hwnd, s_kIdcDriveAudioCheck,
                            state->driveAudioEnabled ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;

        case WM_COMMAND:
            state = reinterpret_cast<OptionsDialogState *> (
                GetWindowLongPtr (hwnd, GWLP_USERDATA));

            if (state == nullptr)
            {
                return FALSE;
            }

            if (LOWORD (wParam) == s_kIdcOkButton)
            {
                state->driveAudioEnabled =
                    (IsDlgButtonChecked (hwnd, s_kIdcDriveAudioCheck) == BST_CHECKED);
                state->accepted = true;
                EndDialog (hwnd, IDOK);
                return TRUE;
            }

            if (LOWORD (wParam) == s_kIdcCancelButton)
            {
                state->accepted = false;
                EndDialog (hwnd, IDCANCEL);
                return TRUE;
            }

            return FALSE;

        case WM_CLOSE:
            EndDialog (hwnd, IDCANCEL);
            return TRUE;
    }

    return FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OptionsDialog::Show (
    HWND       hwndParent,
    HINSTANCE  hInstance,
    bool       currentDriveAudioEnabled,
    bool &     outDriveAudioEnabled)
{
    HRESULT             hr     = S_OK;
    vector<BYTE>        tmpl;
    OptionsDialogState  state  = { currentDriveAudioEnabled, false };
    INT_PTR             result = 0;

    BuildTemplate (tmpl);

    result = DialogBoxIndirectParam (
        hInstance,
        reinterpret_cast<LPCDLGTEMPLATE> (tmpl.data()),
        hwndParent,
        DialogProc,
        reinterpret_cast<LPARAM> (&state));

    if (result == -1)
    {
        hr = HRESULT_FROM_WIN32 (GetLastError());
        goto Error;
    }

    if (state.accepted)
    {
        outDriveAudioEnabled = state.driveAudioEnabled;
        hr = S_OK;
    }
    else
    {
        hr = S_FALSE;
    }

Error:
    return hr;
}
