#pragma once
#include <vector>
#include <string>
#include <functional>

// ============================================================================
//  PlatformAudio — the only two touch points between the library and the OS.
//
//  Platform implementations must:
//  1. Implement AudioFileDecoder to load files into raw PCM
//  2. Implement AudioDevice to drive the fill callback
//
//  Everything else (scheduling, morphisms, mixing math) is pure C++ and
//  lives in the library. The platform never sees it.
// ============================================================================

namespace mtpl {
// ----------------------------------------------------------------------------
//  AudioFileDecoder
//
//  Platform decodes a file to raw interleaved float PCM.
//  Library then resamples to kOutputSampleRate via sincResample.
//
//  decode() must return:
//    samples  — interleaved float: [L0,R0,L1,R1,...] or [M0,M1,...] if mono
//    outSampleRate  — the sample rate of the returned PCM
//    outChannels    — 1 (mono) or 2 (stereo)
// ----------------------------------------------------------------------------

struct DecodedAudio {
    std::vector<float> samples;
    double             sampleRate;
    int                channels;
};

class AudioFileDecoder {
public:
    virtual ~AudioFileDecoder() = default;
    virtual DecodedAudio decode(const std::string& path) = 0;
};

// ----------------------------------------------------------------------------
//  AudioDevice
//
//  Platform owns the hardware audio loop.
//  Library provides a fill callback: (float* buffer, int numFrames) → void
//  Buffer is stereo interleaved float at kOutputSampleRate.
//
//  pause() / resume() stop and restart the hardware loop without resetting
//  the sample cursor — useful for backgrounding on mobile.
// ----------------------------------------------------------------------------

using FillCallback = std::function<void(float*, int)>;

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    // Start the hardware loop, calling fillCallback each time a buffer is needed
    virtual void start(FillCallback fillCallback) = 0;

    // Pause — stop the hardware loop, keep all state
    virtual void pause() = 0;

    // Resume — restart the hardware loop from where it paused
    virtual void resume() = 0;

    // Stop — tear down the hardware loop entirely
    virtual void stop() = 0;
};
}