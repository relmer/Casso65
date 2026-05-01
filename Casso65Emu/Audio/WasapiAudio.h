#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
//  WASAPI shared-mode pull-mode audio streaming.
//  Float32 mono at device mix format sample rate.
//
////////////////////////////////////////////////////////////////////////////////

class WasapiAudio
{
public:
    WasapiAudio ();
    ~WasapiAudio ();

    HRESULT Initialize ();
    void    Shutdown   ();

    // Submit one audio slice from speaker toggle timestamps
    HRESULT SubmitFrame (
        const std::vector<uint32_t> & toggleTimestamps,
        uint32_t totalCyclesThisSlice,
        float currentSpeakerState,
        uint32_t numSamplesToGenerate);

    bool IsInitialized () const { return m_initialized; }
    UINT32 GetSampleRate () const { return m_sampleRate; }

private:
    IMMDeviceEnumerator * m_enumerator;
    IMMDevice *           m_device;
    IAudioClient *        m_audioClient;
    IAudioRenderClient *  m_renderClient;

    UINT32  m_bufferFrames;
    UINT32  m_sampleRate;
    UINT32  m_samplesPerFrame;
    UINT32  m_channels;
    bool    m_initialized;

    // Pending mono samples waiting to be drained into WASAPI
    std::vector<float> m_pendingSamples;
};
