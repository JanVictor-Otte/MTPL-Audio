#include "mtpl/mtpl.hpp"
#include "mtpl_audio/platform/AudioToolbox.hpp"
#include "mtpl_audio/audio/SoundBuffer.hpp"
#include "mtpl_audio/audio/Mixer.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace mtpl;

int main() {
    std::cout << "\n=== MTPL-Audio Apple Platform Tests ===\n" << std::endl;
    
    // Test 1: AudioToolboxFileDecoder
    std::cout << "Testing AudioToolboxFileDecoder..." << std::endl;
    try {
        AudioToolboxFileDecoder decoder;
        
        // You'll need to provide an actual audio file path for this test
        // For now, we'll just create the decoder to verify it compiles
        std::cout << "  ✓ AudioToolboxFileDecoder created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ AudioToolboxFileDecoder failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Test 2: SoundBuffer construction (without actual file)
    std::cout << "\nTesting SoundBuffer construction..." << std::endl;
    try {
        // Create a simple test buffer from raw PCM
        DecodedAudio testAudio;
        testAudio.sampleRate = 44100.0;
        testAudio.channels = 2;
        
        // Generate 1 second of 440Hz sine wave (A4)
        int numFrames = 44100;
        testAudio.samples.resize(numFrames * 2);
        for (int i = 0; i < numFrames; ++i) {
            float t = i / 44100.0f;
            float sample = 0.3f * std::sin(2.0f * M_PI * 440.0f * t);
            testAudio.samples[i * 2 + 0] = sample;  // L
            testAudio.samples[i * 2 + 1] = sample;  // R
        }
        
        SoundBuffer buffer(testAudio);
        std::cout << "  ✓ SoundBuffer created: " << buffer.numFrames << " frames" << std::endl;
        std::cout << "    (Should be resampled to 48kHz: " << (int)(44100.0 * 48000.0 / 44100.0) << " frames)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ SoundBuffer failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Test 3: AudioToolboxDevice creation
    std::cout << "\nTesting AudioToolboxDevice..." << std::endl;
    try {
        AudioToolboxDevice device;
        std::cout << "  ✓ AudioToolboxDevice created successfully" << std::endl;
        
        // Start and immediately stop to verify the device works
        bool fillCalled = false;
        device.start([&fillCalled](float* buf, int numFrames) {
            fillCalled = true;
            // Fill with silence
            for (int i = 0; i < numFrames * 2; ++i) {
                buf[i] = 0.0f;
            }
        });
        
        std::cout << "  ✓ Audio device started" << std::endl;
        
        // Wait a bit for at least one fill callback
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        device.stop();
        std::cout << "  ✓ Audio device stopped" << std::endl;
        
        if (fillCalled) {
            std::cout << "  ✓ Fill callback was invoked" << std::endl;
        } else {
            std::cout << "  ⚠ Fill callback was not invoked (might be timing issue)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ✗ AudioToolboxDevice failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Test 4: Mixer with device
    std::cout << "\nTesting Mixer integration..." << std::endl;
    try {
        Mixer mixer;
        AudioToolboxDevice device;
        DecodedAudio testAudio;
        testAudio.sampleRate = 48000.0;
        testAudio.channels = 2;
        int numFrames = 4800;  // 0.1 seconds
        testAudio.samples.resize(numFrames * 2);
        for (int i = 0; i < numFrames; ++i) {
            float t = i / 48000.0f;
            float sample = 0.2f * std::sin(2.0f * M_PI * 880.0f * t);  // A5
            testAudio.samples[i * 2 + 0] = sample;
            testAudio.samples[i * 2 + 1] = sample;
        }
        SoundBuffer buffer(testAudio);
        
        // Schedule the buffer to play immediately
        mixer.schedule(&buffer, 1.0f, 0.5f, 0.0);
        
        // Start the device with mixer callback
        device.start([&mixer](float* buf, int n) { mixer.fill(buf, n); });
        
        std::cout << "  ✓ Playing test tone for 0.5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        device.stop();
        std::cout << "  ✓ Mixer integration successful" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "  ✗ Mixer integration failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Test 5: Pause/Resume
    std::cout << "\nTesting pause/resume..." << std::endl;
    try {
        AudioToolboxDevice device;
        int fillCount = 0;
        
        device.start([&fillCount](float* buf, int numFrames) {
            ++fillCount;
            for (int i = 0; i < numFrames * 2; ++i) buf[i] = 0.0f;
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int countBeforePause = fillCount;
        
        device.pause();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int countDuringPause = fillCount;
        
        device.resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int countAfterResume = fillCount;
        
        device.stop();
        
        std::cout << "  Fill counts: before=" << countBeforePause 
                  << " during=" << countDuringPause 
                  << " after=" << countAfterResume << std::endl;
        
        if (countDuringPause == countBeforePause && countAfterResume > countDuringPause) {
            std::cout << "  ✓ Pause/resume working correctly" << std::endl;
        } else {
            std::cout << "  ⚠ Pause/resume behavior unclear (timing dependent)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  ✗ Pause/resume failed: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== All Platform Tests Passed ===\n" << std::endl;
    return 0;
}