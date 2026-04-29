#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
//  In-app debug log window (opened via Ctrl+D).
//
////////////////////////////////////////////////////////////////////////////////

class DebugConsole
{
public:
    DebugConsole ();

    void Show ();
    void Hide ();
    bool IsVisible () const { return m_visible; }

    void Log (const std::wstring & message);
    void LogConfig (const std::string & summary);

private:
    bool    m_visible;
    HWND    m_hwnd;
};
