#pragma once

#include "mtpl_audio/audio/SoundBuffer.hpp"
#include "mtpl_audio/audio/PlatformAudio.hpp"

#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>

namespace mtpl {
// ============================================================================
//  Voice — one active sound being mixed
// ============================================================================

struct Voice {
    const SoundBuffer* buffer  = nullptr;
    double             readPos = 0.0;
    double             advance = 1.0;
    float              gain    = 1.0f;
    bool               done    = false;

    bool mix(float* outL, float* outR, int numFrames) {
        for (int i = 0; i < numFrames; ++i) {
            if (readPos >= buffer->numFrames - 1) { done = true; return false; }
            int    f0   = (int)readPos;
            int    f1   = std::min(f0 + 1, buffer->numFrames - 1);
            double frac = readPos - f0;
            float  l0   = buffer->samples[f0 * 2 + 0];
            float  r0   = buffer->samples[f0 * 2 + 1];
            float  l1   = buffer->samples[f1 * 2 + 0];
            float  r1   = buffer->samples[f1 * 2 + 1];
            outL[i] += gain * (float)(l0 + frac * (l1 - l0));
            outR[i] += gain * (float)(r0 + frac * (r1 - r0));
            readPos += advance;
        }
        return true;
    }
};

struct ScheduledVoice {
    Voice    voice;
    uint64_t startSample;
};

// ============================================================================
//  Mixer — pure C++, no platform dependencies
//
//  The platform calls fill(buf, numFrames) whenever it needs audio.
//  The scheduler calls schedule() to queue sounds.
//  The platform drives the loop via AudioDevice.
// ============================================================================

class Mixer {
public:
    static constexpr int kBufferFrames = 2048;

    // Called by the platform — fill buf with numFrames stereo interleaved float
    void fill(float* buf, int numFrames) {
        std::vector<float> mixL(numFrames, 0.f);
        std::vector<float> mixR(numFrames, 0.f);

        std::lock_guard<std::mutex> lock(mutex_);

        uint64_t bufStart = sampleCursor_;
        uint64_t bufEnd   = sampleCursor_ + numFrames;

        // Activate pending voices whose start falls in this buffer
        for (auto& sv : pending_) {
            if (sv.startSample < bufEnd) {
                int offset = (sv.startSample >= bufStart)
                    ? (int)(sv.startSample - bufStart) : 0;
                offsetActive_.push_back({ sv.voice, offset });
            }
        }
        pending_.erase(
            std::remove_if(pending_.begin(), pending_.end(),
                [bufEnd](const ScheduledVoice& sv){ return sv.startSample < bufEnd; }),
            pending_.end());

        // Mix active voices
        for (auto& v : active_)
            v.mix(mixL.data(), mixR.data(), numFrames);
        active_.erase(
            std::remove_if(active_.begin(), active_.end(),
                [](const Voice& v){ return v.done; }),
            active_.end());

        // Mix newly activated voices
        for (auto& [v, offset] : offsetActive_) {
            v.mix(mixL.data() + offset, mixR.data() + offset, numFrames - offset);
            if (!v.done) active_.push_back(v);
        }
        offsetActive_.clear();

        sampleCursor_ += numFrames;

        // Interleave and clamp
        for (int i = 0; i < numFrames; ++i) {
            buf[i * 2 + 0] = std::max(-1.f, std::min(1.f, mixL[i]));
            buf[i * 2 + 1] = std::max(-1.f, std::min(1.f, mixR[i]));
        }
    }

    void schedule(const SoundBuffer* buffer, float pitch, float gain,
                  double absoluteTimeSeconds)
    {
        ScheduledVoice sv;
        sv.startSample   = (uint64_t)(absoluteTimeSeconds * kOutputSampleRate);
        sv.voice.buffer  = buffer;
        sv.voice.readPos = 0.0;
        sv.voice.advance = (double)pitch;
        sv.voice.gain    = gain;
        sv.voice.done    = false;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(sv);
    }

    // Pause — freeze the sample cursor, platform stops calling fill()
    // No state is lost. resume() picks up exactly where it left off.
    void pause() { /* cursor frozen, platform stops calling fill */ }

    // Reset cursor — call when starting fresh after a full stop
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        sampleCursor_ = 0;
        pending_.clear();
        active_.clear();
        offsetActive_.clear();
    }

private:
    std::mutex                  mutex_;
    std::vector<ScheduledVoice> pending_;
    std::vector<Voice>          active_;
    uint64_t                    sampleCursor_ = 0;

    struct OffsetVoice { Voice v; int offset; };
    std::vector<OffsetVoice> offsetActive_;
};
}