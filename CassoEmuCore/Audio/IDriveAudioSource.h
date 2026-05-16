#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioSource
//
//  Abstract per-drive audio source. Owns the sample set (or synthesis
//  state) for one physical drive instance, plus its equal-power pan
//  coefficients and play state. Implements IDriveAudioSink so the
//  drive controller can notify it directly; produces mono float PCM
//  on demand for the mixer to pan into stereo.
//
//  Spec reference: FR-012 (per-drive equal-power pan), FR-016
//  (generic abstraction).
//
////////////////////////////////////////////////////////////////////////////////

class IDriveAudioSource : public IDriveAudioSink
{
public:
    ~IDriveAudioSource() override = default;

    // Produce `numSamples` of mono float PCM into `outMono`. The mixer
    // pans the output per-channel via PanLeft() / PanRight(). The source
    // is responsible for clearing the buffer (the mixer assumes the
    // returned values are additive contributions only, not pre-mixed
    // with anything else).
    virtual void  GeneratePCM (float * outMono, uint32_t numSamples) = 0;

    // Per-drive equal-power pan coefficients (panL^2 + panR^2 == 1).
    // Centered: panL == panR == sqrt(0.5) ~= 0.707.
    virtual float PanLeft     () const = 0;
    virtual float PanRight    () const = 0;

    // Caller-provided pan: stores the precomputed coefficients
    // directly. Callers that want angle-based placement compute
    // panL = cos(theta), panR = sin(theta) once and pass them here.
    virtual void  SetPan      (float panLeft, float panRight) = 0;
};
