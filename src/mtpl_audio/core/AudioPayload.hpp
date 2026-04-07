#pragma once
#include "mtpl/timed/TimedEvent.hpp"
#include <memory>


namespace mtpl {

struct SoundBuffer;

struct PreloadedGeneratorBase { 
    virtual ~PreloadedGeneratorBase() = default; 

    struct SampledVariant {
        const SoundBuffer* buffer;
        float              attackTime;
    };
    
    virtual SampledVariant sample() const = 0;  // Pure virtual

};

// ============================================================================
//  AudioPayload — concrete payload for the audio domain
//  AudioEvent   — TimedEvent<AudioPayload>
//  Audio layer aliases
// ============================================================================

struct AudioPayload {
    std::shared_ptr<PreloadedGeneratorBase> generator;
    float pitch = 1.0f;
    float gain  = 1.0f;
};

using AudioEvent     = TimedEvent<AudioPayload>;
using AudioLane      = Lane<AudioEvent>;
using AudioMultiLane = MultiLane<AudioEvent>;
using AudioMorphism  = Morphism<AudioEvent>;

// Signal aliases: E = AudioEvent
template<typename T> using AudioFrozenSignal         = FrozenSignal<T, AudioEvent>;
template<typename T> using AudioSignal               = Signal<T, AudioEvent>;
template<typename T> using ConstantFrozenAudioSignal = ConstantFrozenSignal<T, AudioEvent>;
template<typename T> using ConstantAudioSignal       = ConstantSignal<T, AudioEvent>;
}