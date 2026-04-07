// Compilation test for mtpl_audio architecture rewrite.
// Exercises the header include chain and basic instantiation
// without requiring platform audio (AudioToolbox) or file I/O.

#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/core/AudioSignal.hpp"
#include "mtpl_audio/core/AudioSource.hpp"
#include "mtpl_audio/audio/Generator.hpp"
#include "mtpl_audio/audio/Pitch.hpp"
#include "mtpl_audio/audio/Dynamics.hpp"
#include "mtpl_audio/audio/Rhythm.hpp"
#include "mtpl_audio/audio/Mixer.hpp"
// Note: Scheduler.hpp and PreloadedGenerator.hpp need AudioFileDecoder
// which is platform-specific, but they're header-only so we include them
// through AudioSource.hpp already.

#include <iostream>
#include <cassert>
#include <cmath>

using namespace mtpl;

// ============================================================================
//  Helpers
// ============================================================================

template<typename T>
void assert_near(T a, T b, T eps, const char* msg) {
    if (std::abs(a - b) > eps) {
        std::cerr << "FAIL: " << msg << " — got " << a << ", expected " << b << std::endl;
        throw std::runtime_error(msg);
    }
    std::cout << "  ✓ " << msg << std::endl;
}

void assert_true(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
        throw std::runtime_error(msg);
    }
    std::cout << "  ✓ " << msg << std::endl;
}

// ============================================================================
//  1. Constant signal via new API
// ============================================================================

void test_constant_signal() {
    std::cout << "\n=== Constant signal ===" << std::endl;
    auto c = constant(42.0f);
    auto frozen = c();
    AudioEvent e{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}};
    assert_near(frozen(e), 42.0f, 0.001f, "constant(42) == 42");
}

// ============================================================================
//  2. Signal arithmetic (constant path)
// ============================================================================

void test_signal_arithmetic() {
    std::cout << "\n=== Signal arithmetic ===" << std::endl;
    auto a = constant(10.0f);
    auto b = constant(3.0f);
    auto sum = a + b;
    AudioEvent e{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}};
    auto frozen = sum();
    assert_near(frozen(e), 13.0f, 0.001f, "10 + 3 = 13");
}

// ============================================================================
//  3. Pitch EventLeaf
// ============================================================================

void test_pitch_morphism() {
    std::cout << "\n=== Pitch EventLeaf ===" << std::endl;
    auto m = pitch(12.0f); // up one octave: pitch *= 2
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    assert_near(output[0][0].payload.pitch, 2.0f, 0.01f, "pitch(12) doubles pitch");
}

// ============================================================================
//  4. Gain EventLeaf
// ============================================================================

void test_gain_morphism() {
    std::cout << "\n=== Gain EventLeaf ===" << std::endl;
    auto m = gain(0.5f);
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    assert_near(output[0][0].payload.gain, 0.5f, 0.001f, "gain(0.5) halves gain");
}

// ============================================================================
//  5. GainDb EventLeaf
// ============================================================================

void test_gaindb_morphism() {
    std::cout << "\n=== GainDb EventLeaf ===" << std::endl;
    auto m = gainDb(-20.0f); // -20dB = gain * 0.1
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    assert_near(output[0][0].payload.gain, 0.1f, 0.01f, "gainDb(-20) = 0.1×");
}

// ============================================================================
//  6. Envelope MultiLaneLeaf
// ============================================================================

void test_envelope_morphism() {
    std::cout << "\n=== Envelope MultiLaneLeaf ===" << std::endl;
    auto m = envelope(0.0f, 1.0f); // fade in
    AudioMultiLane input = {
        AudioLane{
            AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}},
            AudioEvent{0.1f, AudioPayload{nullptr, 1.0f, 1.0f}},
            AudioEvent{0.2f, AudioPayload{nullptr, 1.0f, 1.0f}}
        }
    };
    auto output = m(input);
    assert_near(output[0][0].payload.gain, 0.0f, 0.001f, "envelope start = 0");
    assert_near(output[0][1].payload.gain, 0.5f, 0.001f, "envelope mid = 0.5");
    assert_near(output[0][2].payload.gain, 1.0f, 0.001f, "envelope end = 1");
}

// ============================================================================
//  7. TimeOffset EventLeaf
// ============================================================================

void test_timeoffset_morphism() {
    std::cout << "\n=== TimeOffset EventLeaf ===" << std::endl;
    auto m = timeOffset(0.5f);
    AudioMultiLane input = {
        AudioLane{ AudioEvent{1.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    assert_near(output[0][0].time, 1.5f, 0.001f, "timeOffset(0.5): 1.0 → 1.5");
}

// ============================================================================
//  8. Compose (humanize is a Compose)
// ============================================================================

void test_humanize_compose() {
    std::cout << "\n=== Humanize Compose ===" << std::endl;
    auto m = humanize(0.01f, 0.05f, 0.5f);
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    // Just verify it runs without crashing
    auto output = m(input);
    assert_true(output.size() == 1, "humanize preserves lane count");
    assert_true(output[0].size() == 1, "humanize preserves event count");
    std::cout << "  ✓ humanize runs without crash" << std::endl;
}

// ============================================================================
//  9. Varying signal (randomUniform) with EventLeaf
// ============================================================================

void test_varying_signal_with_morphism() {
    std::cout << "\n=== Varying signal + EventLeaf ===" << std::endl;
    auto randGain = randomUniform(0.5f, 1.0f);
    auto m = gain(randGain);
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    float g = output[0][0].payload.gain;
    assert_true(g >= 0.0f && g <= 1.5f, "gain with randomUniform in plausible range");
}

// ============================================================================
//  10. Detune EventLeaf
// ============================================================================

void test_detune_morphism() {
    std::cout << "\n=== Detune EventLeaf ===" << std::endl;
    auto m = detune(1.0f); // ±1 semitone
    AudioMultiLane input = {
        AudioLane{ AudioEvent{0.0f, AudioPayload{nullptr, 1.0f, 1.0f}} }
    };
    auto output = m(input);
    float p = output[0][0].payload.pitch;
    // ±1 semitone from 1.0 = range [2^(-1/12), 2^(1/12)] ≈ [0.944, 1.059]
    assert_true(p > 0.9f && p < 1.1f, "detune(1) pitch in plausible range");
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    try {
        std::cout << "=================================" << std::endl;
        std::cout << "Audio Architecture Tests" << std::endl;
        std::cout << "=================================" << std::endl;

        test_constant_signal();
        test_signal_arithmetic();
        test_pitch_morphism();
        test_gain_morphism();
        test_gaindb_morphism();
        test_envelope_morphism();
        test_timeoffset_morphism();
        test_humanize_compose();
        test_varying_signal_with_morphism();
        test_detune_morphism();

        std::cout << "\n=================================" << std::endl;
        std::cout << "ALL AUDIO TESTS PASSED!" << std::endl;
        std::cout << "=================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
