#pragma once

#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/MachineConfig.h"
#include "Core/ComponentRegistry.h"
#include "Shell/D3DRenderer.h"
#include "Shell/MenuSystem.h"
#include "Shell/DebugConsole.h"
#include "Video/VideoOutput.h"
#include "Audio/WasapiAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommand
//
//  Commands queued from the UI thread for the CPU thread to execute.
//
////////////////////////////////////////////////////////////////////////////////

struct EmulatorCommand
{
    WORD  id;
    std::string payload;
};



////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell
{
public:
    EmulatorShell ();
    ~EmulatorShell ();

    HRESULT Initialize (HINSTANCE hInstance, const MachineConfig & config,
                        const std::string & disk1Path,
                        const std::string & disk2Path);

    int RunMessageLoop ();

    // Window procedure (static + instance)
    static LRESULT CALLBACK StaticWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void HandleCommand (WORD commandId);

    // Keyboard input from WM_KEYDOWN/WM_CHAR
    void OnKeyDown (WPARAM vk, LPARAM lParam);
    void OnKeyUp   (WPARAM vk, LPARAM lParam);
    void OnChar    (WPARAM ch);

    // State
    bool IsRunning () const { return m_running.load (std::memory_order_acquire); }
    bool IsPaused  () const { return m_paused.load (std::memory_order_acquire); }

    // Access bus for test wiring
    MemoryBus & GetBus () { return m_memoryBus; }

private:
    // CPU thread entry point and helpers
    void CpuThreadProc     ();
    void RunOneFrame       ();
    void ProcessCommands   ();
    void UpdateWindowTitle ();
    void SelectVideoMode   ();

    // Queue a command for the CPU thread
    void PostCommand (WORD id, const std::string & payload = "");

    HWND                m_hwnd;
    HINSTANCE           m_hInstance;
    HACCEL              m_accelTable;

    MemoryBus           m_memoryBus;
    ComponentRegistry   m_registry;
    std::unique_ptr<EmuCpu> m_cpu;

    D3DRenderer         m_d3dRenderer;
    MenuSystem          m_menuSystem;
    WasapiAudio         m_wasapiAudio;
    DebugConsole        m_debugConsole;

    // Owned devices
    std::vector<std::unique_ptr<MemoryDevice>> m_ownedDevices;

    // Video
    std::vector<std::unique_ptr<VideoOutput>> m_videoModes;
    VideoOutput *       m_activeVideoMode;

    // Soft switch state (read by video mode selection)
    bool    m_graphicsMode;
    bool    m_mixedMode;
    bool    m_page2;
    bool    m_hiresMode;
    bool    m_col80Mode;
    bool    m_doubleHiRes;

    // Device pointers (non-owning, for quick access)
    class AppleKeyboard *         m_keyboard;
    class AppleSoftSwitchBank *   m_softSwitches;
    class AppleSpeaker *          m_speaker;

    // Emulation state
    MachineConfig   m_config;

    // -- Threading --
    std::thread     m_cpuThread;

    // Atomic flags (UI writes, CPU reads)
    std::atomic<bool>       m_running;
    std::atomic<bool>       m_paused;
    std::atomic<SpeedMode>  m_speedMode;
    std::atomic<ColorMode>  m_colorMode;

    // Command queue (UI → CPU, protected by m_cmdMutex)
    std::mutex                    m_cmdMutex;
    std::vector<EmulatorCommand>  m_commandQueue;

    // Double framebuffer (CPU renders, UI presents, protected by m_fbMutex)
    std::mutex              m_fbMutex;
    std::vector<uint32_t>   m_cpuFramebuffer;
    std::vector<uint32_t>   m_uiFramebuffer;
    bool                    m_fbReady;

    uint32_t        m_cyclesPerFrame;
    double          m_sampleRemainder;
};
