#include "Pch.h"

#include "Ehm.h"



EHM_BREAKPOINT_FUNC g_pfnBreakpoint = nullptr;



void SetBreakpointFunction (EHM_BREAKPOINT_FUNC func)
{
    g_pfnBreakpoint = func;
}



void EhmBreakpoint (void)
{
#ifdef _WINDOWS_
    g_pfnBreakpoint ? g_pfnBreakpoint() : __debugbreak();
#elif defined(_MSC_VER)
    g_pfnBreakpoint ? g_pfnBreakpoint() : __debugbreak();
#else
    if (g_pfnBreakpoint)
    {
        g_pfnBreakpoint();
    }
#endif
}



#ifdef _WINDOWS_

//
// Windows path — OutputDebugString + StringCchVPrintf
//

void DEBUGMSG (LPCWSTR pszFormat, ...)
{
#ifdef _DEBUG
    va_list vlArgs;
    WCHAR   szMsg[1024];

    va_start (vlArgs, pszFormat);
    HRESULT hr = StringCchVPrintf (szMsg, ARRAYSIZE (szMsg), pszFormat, vlArgs);
    if (SUCCEEDED (hr))
    {
        OutputDebugString (szMsg);
    }
    va_end (vlArgs);
#else
    UNREFERENCED_PARAMETER (pszFormat);
#endif
}



void RELEASEMSG (LPCWSTR pszFormat, ...)
{
    va_list vlArgs;
    WCHAR   szMsg[1024];

    va_start (vlArgs, pszFormat);
    HRESULT hr = StringCchVPrintf (szMsg, ARRAYSIZE (szMsg), pszFormat, vlArgs);
    if (SUCCEEDED (hr))
    {
        OutputDebugString (szMsg);
    }
    va_end (vlArgs);
}

#else

//
// Portable C++ path — fprintf to stderr
//

void DEBUGMSG (const char * pszFormat, ...)
{
#ifdef _DEBUG
    va_list vlArgs;

    va_start (vlArgs, pszFormat);
    std::vfprintf (stderr, pszFormat, vlArgs);
    va_end (vlArgs);
#else
    (void) pszFormat;
#endif
}



void RELEASEMSG (const char * pszFormat, ...)
{
    va_list vlArgs;

    va_start (vlArgs, pszFormat);
    std::vfprintf (stderr, pszFormat, vlArgs);
    va_end (vlArgs);
}

#endif
