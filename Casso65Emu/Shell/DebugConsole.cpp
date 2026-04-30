#include "Pch.h"

#include "DebugConsole.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugConsole
//
////////////////////////////////////////////////////////////////////////////////

DebugConsole::DebugConsole ()
    : m_visible (false),
      m_hwnd    (nullptr)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Show ()
{
    if (m_visible)
    {
        return;
    }

    if (AllocConsole ())
    {
        // Prevent the console's X button from killing the emulator.
        // Windows sends CTRL_CLOSE_EVENT when the user closes the console
        // window; without a handler the entire process terminates.
        SetConsoleCtrlHandler ([](DWORD ctrlType) -> BOOL
        {
            if (ctrlType == CTRL_CLOSE_EVENT)
            {
                FreeConsole ();
                return TRUE;
            }
            return FALSE;
        }, TRUE);

        FILE * fp = nullptr;
        freopen_s (&fp, "CONOUT$", "w", stdout);
        freopen_s (&fp, "CONOUT$", "w", stderr);

        SetConsoleTitleW (L"Casso65 Debug Console");
        m_hwnd = GetConsoleWindow ();
        m_visible = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Hide ()
{
    if (!m_visible)
    {
        return;
    }

    FreeConsole ();
    m_visible = false;
    m_hwnd    = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Log
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::Log (const std::wstring & message)
{
    if (!m_visible)
    {
        return;
    }

    wprintf (L"%s\n", message.c_str ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LogConfig
//
////////////////////////////////////////////////////////////////////////////////

void DebugConsole::LogConfig (const std::string & summary)
{
    if (!m_visible)
    {
        return;
    }

    printf ("%s\n", summary.c_str ());
}
