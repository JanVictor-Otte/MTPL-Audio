#pragma once
#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/core/AudioSignal.hpp"
#include <cmath>
#include <random>

namespace mtpl {

// pitch — per-event: multiply pitch by 2^(semitones/12)
inline EventLeaf<AudioEvent, float> pitch(AudioSignal<float> semitones) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float s) -> AudioEvent {
            e.payload.pitch *= std::pow(2.f, s / 12.f);
            return e;
        },
        semitones
    );
}
inline EventLeaf<AudioEvent, float> pitch(float v) { return pitch(constant(v)); }

// pitchMultiply — per-event: multiply pitch by factor
inline EventLeaf<AudioEvent, float> pitchMultiply(AudioSignal<float> factor) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float f) -> AudioEvent {
            e.payload.pitch *= f;
            return e;
        },
        factor
    );
}
inline EventLeaf<AudioEvent, float> pitchMultiply(float v) { return pitchMultiply(constant(v)); }

// detune — per-event: random pitch deviation within ±maxSemitones
inline EventLeaf<AudioEvent, float> detune(AudioSignal<float> maxSemitones) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float maxS) -> AudioEvent {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(-maxS, maxS);
            e.payload.pitch *= std::pow(2.f, dist(rng) / 12.f);
            return e;
        },
        maxSemitones
    );
}
inline EventLeaf<AudioEvent, float> detune(float v) { return detune(constant(v)); }
}