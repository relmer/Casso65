#include "Pch.h"

#include "WasapiAudio.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::WasapiAudio ()
    : m_enumerator   (nullptr),
      m_device       (nullptr),
      m_audioClient  (nullptr),
      m_renderClient (nullptr),
      m_bufferFrames (0),
      m_sampleRate   (44100),
      m_initialized  (false)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::~WasapiAudio ()
{
    Shutdown ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::Initialize ()
{
    HRESULT hr = S_OK;

    // Create device enumerator
    hr = CoCreateInstance (
        __uuidof (MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof (IMMDeviceEnumerator),
        reinterpret_cast<void **> (&m_enumerator));
    CHR (hr);

    // Get default audio endpoint
    hr = m_enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &m_device);
    CHR (hr);

    // Activate audio client
    hr = m_device->Activate (
        __uuidof (IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **> (&m_audioClient));
    CHR (hr);

    // Get mix format and try float32 mono
    {
        WAVEFORMATEX * mixFormat = nullptr;
        hr = m_audioClient->GetMixFormat (&mixFormat);
        CHR (hr);

        m_sampleRate = mixFormat->nSamplesPerSec;

        // Try float32 mono at the mix format sample rate
        WAVEFORMATEX desiredFormat = {};
        desiredFormat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        desiredFormat.nChannels       = 1;
        desiredFormat.nSamplesPerSec  = m_sampleRate;
        desiredFormat.wBitsPerSample  = 32;
        desiredFormat.nBlockAlign     = 4;
        desiredFormat.nAvgBytesPerSec = m_sampleRate * 4;

        REFERENCE_TIME bufferDuration = 330000;  // ~33ms

        hr = m_audioClient->Initialize (
            AUDCLNT_SHAREMODE_SHARED,
            0,
            bufferDuration,
            0,
            &desiredFormat,
            nullptr);

        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
        {
            // Fallback: use mix format directly
            hr = m_audioClient->Initialize (
                AUDCLNT_SHAREMODE_SHARED,
                0,
                bufferDuration,
                0,
                mixFormat,
                nullptr);
        }

        CoTaskMemFree (mixFormat);
        CHR (hr);
    }

    // Get buffer size and render client
    hr = m_audioClient->GetBufferSize (&m_bufferFrames);
    CHR (hr);

    hr = m_audioClient->GetService (
        __uuidof (IAudioRenderClient),
        reinterpret_cast<void **> (&m_renderClient));
    CHR (hr);

    // Start the audio stream
    hr = m_audioClient->Start ();
    CHR (hr);

    m_initialized = true;

Error:
    if (FAILED (hr))
    {
        DEBUGMSG (L"WASAPI initialization failed (hr=0x%08X). Audio disabled.\n", hr);
        Shutdown ();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::Shutdown ()
{
    if (m_audioClient)
    {
        m_audioClient->Stop ();
    }

    if (m_renderClient) { m_renderClient->Release (); m_renderClient = nullptr; }
    if (m_audioClient)  { m_audioClient->Release ();  m_audioClient  = nullptr; }
    if (m_device)       { m_device->Release ();       m_device       = nullptr; }
    if (m_enumerator)   { m_enumerator->Release ();   m_enumerator   = nullptr; }

    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::SubmitFrame (
    const std::vector<uint32_t> & toggleTimestamps,
    uint32_t totalCyclesThisFrame,
    float currentSpeakerState)
{
    HRESULT hr = S_OK;

    if (!m_initialized || m_renderClient == nullptr)
    {
        return S_OK;
    }

    // Get available buffer space
    {
        UINT32 padding = 0;
        hr = m_audioClient->GetCurrentPadding (&padding);
        CHR (hr);

        UINT32 available = m_bufferFrames - padding;

        if (available == 0)
        {
            return S_OK;
        }

        // Get buffer
        BYTE * buffer = nullptr;
        hr = m_renderClient->GetBuffer (available, &buffer);
        CHR (hr);

        {
            float * samples = reinterpret_cast<float *> (buffer);
            float state = currentSpeakerState;

            if (toggleTimestamps.empty () || totalCyclesThisFrame == 0)
            {
                // No toggles — fill with current state (silence or DC)
                for (UINT32 i = 0; i < available; i++)
                {
                    samples[i] = state;
                }
            }
            else
            {
                // Convert toggle timestamps to sample positions
                size_t toggleIdx = 0;
                float toggleState = -state;  // Start with opposite (first toggle produces current)

                for (UINT32 i = 0; i < available; i++)
                {
                    // Map sample position to cycle count
                    uint32_t sampleCycle = static_cast<uint32_t> (
                        static_cast<uint64_t> (i) * totalCyclesThisFrame / available);

                    // Process any toggles up to this sample
                    while (toggleIdx < toggleTimestamps.size () &&
                           toggleTimestamps[toggleIdx] <= sampleCycle)
                    {
                        toggleState = -toggleState;
                        toggleIdx++;
                    }

                    samples[i] = toggleState;
                }
            }
        }

        hr = m_renderClient->ReleaseBuffer (available, 0);
        CHR (hr);
    }

Error:
    return hr;
}
