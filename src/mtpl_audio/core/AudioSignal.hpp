#pragma once
#include "mtpl_audio/core/AudioPayload.hpp"
#include <random>
#include <cmath>
#include <chrono>
#include <memory>

// ============================================================================
//  Audio-layer signal primitives.
//  AudioSignal<T>       = Signal<T, AudioEvent>       (from core)
//  AudioFrozenSignal<T> = FrozenSignal<T, AudioEvent> (from core)
//
//  Signal<T,E> wraps a factory () → FrozenSignal<T,E> in an Element
//  transform. The lambda-factory pattern below is directly compatible:
//  the outer lambda IS the factory; the inner lambda is implicitly
//  convertible to FrozenSignal via its non-explicit constructor.
// ============================================================================


namespace mtpl {

// convenience: audio-context constant — returns ConstantSignal (IS-A Signal)
template<typename T>
ConstantSignal<T, AudioEvent> constant(T value) {
    return SignalTransform<T, AudioEvent>::Constant(value);
}

// convenience: constant from varying signal — freezes at E{}
template<typename T>
ConstantSignal<T, AudioEvent> constant(Signal<T, AudioEvent> s) {
    return SignalTransform<T, AudioEvent>::Constant(s);
}






// ============================================================================
//  randomBernoulli
// ============================================================================
inline AudioSignal<int> randomBernoulli(AudioSignal<float> p) {
    return [p]() {
        auto fp = p();
        return [fp](const AudioEvent& e) -> int {
            static std::mt19937 rng(std::random_device{}());
            return std::bernoulli_distribution(fp(e))(rng);
        };
    };
}

inline AudioSignal<int> randomBernoulli(float p)                { return randomBernoulli(constant(p)); }


// ============================================================================
//  randomUniform
// ============================================================================
inline AudioSignal<float> randomUniform(AudioSignal<float> lo, AudioSignal<float> hi) {
    return [lo, hi]() {
        auto flo = lo(), fhi = hi();
        return [flo, fhi](const AudioEvent& e) -> float {
            static std::mt19937 rng(std::random_device{}());
            float a = flo(e), b = fhi(e);
            if (a > b) std::swap(a, b);
            return std::uniform_real_distribution<float>(a, b)(rng);
        };
    };
}
inline AudioSignal<float> randomUniform(float lo, float hi)                { return randomUniform(constant(lo), constant(hi)); }
inline AudioSignal<float> randomUniform(AudioSignal<float> lo, float hi)   { return randomUniform(lo, constant(hi)); }
inline AudioSignal<float> randomUniform(float lo, AudioSignal<float> hi)   { return randomUniform(constant(lo), hi); }



// ============================================================================
//  randomNormal
// ============================================================================
inline AudioSignal<float> randomNormal(AudioSignal<float> mean, AudioSignal<float> sd) {
    return [mean, sd]() {
        auto fm = mean(), fs = sd();
        return [fm, fs](const AudioEvent& e) -> float {
            static std::mt19937 rng(std::random_device{}());
            return std::normal_distribution<float>(fm(e), std::max(0.f, fs(e)))(rng);
        };
    };
}
inline AudioSignal<float> randomNormal(float m, float s)                     { return randomNormal(constant(m), constant(s)); }
inline AudioSignal<float> randomNormal(AudioSignal<float> m, float s)        { return randomNormal(m, constant(s)); }
inline AudioSignal<float> randomNormal(float m, AudioSignal<float> s)        { return randomNormal(constant(m), s); }

// ============================================================================
//  timeSinceStart, evaluationCount
// ============================================================================
inline AudioSignal<float> timeSinceStart() {
    return []() {
        auto t0 = std::make_shared<std::chrono::steady_clock::time_point>(
            std::chrono::steady_clock::now());
        return [t0](const AudioEvent&) -> float {
            return std::chrono::duration<float>(std::chrono::steady_clock::now() - *t0).count();
        };
    };
}
inline AudioSignal<float> evaluationCount() {
    return []() {
        auto n = std::make_shared<int>(0);
        return [n](const AudioEvent&) -> float { return float((*n)++); };
    };
}

// ============================================================================
//  Event field readers — time is on AudioEvent directly
// ============================================================================
inline AudioSignal<float> fromTime()  { return []() { return [](const AudioEvent& e) -> float { return e.time;          }; }; }
inline AudioSignal<float> fromPitch() { return []() { return [](const AudioEvent& e) -> float { return e.payload.pitch; }; }; }
inline AudioSignal<float> fromGain()  { return []() { return [](const AudioEvent& e) -> float { return e.payload.gain;  }; }; }

// ============================================================================
//  ramp
// ============================================================================
inline AudioSignal<float> ramp(AudioSignal<float> slope, AudioSignal<float> offset) {
    return [slope, offset]() {
        auto fs = slope(), fo = offset();
        auto t0 = std::make_shared<std::chrono::steady_clock::time_point>(
            std::chrono::steady_clock::now());
        return [fs, fo, t0](const AudioEvent& e) -> float {
            float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - *t0).count();
            return fs(e) * t + fo(e);
        };
    };
}
inline AudioSignal<float> ramp(float s, float o = 0.f)        { return ramp(constant(s), constant(o)); }
inline AudioSignal<float> ramp(AudioSignal<float> s, float o)  { return ramp(s, constant(o)); }
inline AudioSignal<float> ramp(float s, AudioSignal<float> o)  { return ramp(constant(s), o); }

// ============================================================================
//  clampSignal
// ============================================================================
inline AudioSignal<float> clampSignal(float lo, float hi, AudioSignal<float> s) {
    return [lo, hi, s]() {
        auto fs = s();
        return [lo, hi, fs](const AudioEvent& e) -> float {
            return std::max(lo, std::min(hi, fs(e)));
        };
    };
}

// ============================================================================
//  everyNth - freezes to constant 1 every n-th evalution, else 0. 
// ============================================================================
inline ConstantAudioSignal<int> everyNth(ConstantAudioSignal<int> n) {
    auto count = std::make_shared<int>(0);
    auto n_curr = std::make_shared<ConstantFrozenAudioSignal<int>>(1);
    return constant(AudioSignal<int>([n, count, n_curr]() {
        int nth = ((*count)++ % (*n_curr)()) == 0 ? 1 : 0;
        if (nth == 1) {
            *n_curr = n();
        }
        return [nth](const AudioEvent&) -> int {
            return nth;
        };
    }));
}

inline ConstantAudioSignal<int> everyNth(int n) {
    return everyNth(constant(n));
}

}