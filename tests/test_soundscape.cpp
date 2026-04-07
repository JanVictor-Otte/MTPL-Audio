#include "mtpl/mtpl.hpp"
#include "mtpl_audio/platform/AudioToolbox.hpp"
#include "mtpl_audio/audio/SoundBuffer.hpp"
#include "mtpl_audio/audio/Mixer.hpp"
#include "mtpl_audio/audio/Scheduler.hpp"
#include "mtpl_audio/audio/Generator.hpp"
#include "mtpl_audio/audio/Dynamics.hpp"
#include "mtpl_audio/audio/Pitch.hpp"
#include "mtpl_audio/audio/Rhythm.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <fstream>

using namespace mtpl;

// Write a simple WAV file
void writeWAV(const std::string& filename, const std::vector<float>& samples, 
              int sampleRate, int channels) {
    std::ofstream file(filename, std::ios::binary);
    
    int numFrames = samples.size() / channels;
    int dataSize = samples.size() * sizeof(int16_t);
    
    // WAV header
    file.write("RIFF", 4);
    int chunkSize = 36 + dataSize;
    file.write((char*)&chunkSize, 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    file.write((char*)&subchunk1Size, 4);
    short audioFormat = 1; // PCM
    file.write((char*)&audioFormat, 2);
    short numChannels = channels;
    file.write((char*)&numChannels, 2);
    file.write((char*)&sampleRate, 4);
    int byteRate = sampleRate * channels * sizeof(int16_t);
    file.write((char*)&byteRate, 4);
    short blockAlign = channels * sizeof(int16_t);
    file.write((char*)&blockAlign, 2);
    short bitsPerSample = 16;
    file.write((char*)&bitsPerSample, 2);
    file.write("data", 4);
    file.write((char*)&dataSize, 4);
    
    // Convert float to int16 and write
    for (float sample : samples) {
        int16_t s = (int16_t)(sample * 32767.0f);
        file.write((char*)&s, sizeof(int16_t));
    }
}

// Generate a tone and save as WAV
std::string generateToneFile(const std::string& name, float frequency, 
                             float duration, float sampleRate = 48000.0f) {
    std::string filename = "/tmp/" + name + ".wav";
    
    int numFrames = (int)(duration * sampleRate);
    std::vector<float> samples(numFrames * 2);
    
    for (int i = 0; i < numFrames; ++i) {
        float t = i / sampleRate;
        // Envelope
        float envelope = 1.0f;
        float attackTime = 0.005f;
        float releaseTime = 0.01f;
        if (t < attackTime) {
            envelope = t / attackTime;
        } else if (t > duration - releaseTime) {
            envelope = (duration - t) / releaseTime;
        }
        
        float sample = 0.3f * envelope * std::sin(2.0f * M_PI * frequency * t);
        samples[i * 2 + 0] = sample;
        samples[i * 2 + 1] = sample;
    }
    
    writeWAV(filename, samples, (int)sampleRate, 2);
    return filename;
}

int main() {
    std::cout << "\n=== MTPL-Audio Soundscape Test ===\n" << std::endl;
    
    // Generate temporary WAV files
    std::cout << "Generating synthetic sounds..." << std::endl;
    std::string clickHigh = generateToneFile("click_high", 1000.0f, 0.05f);
    std::string clickMid  = generateToneFile("click_mid",  800.0f,  0.05f);
    std::string clickLow  = generateToneFile("click_low",  600.0f,  0.05f);
    std::string bass      = generateToneFile("bass",       110.0f,  0.2f);
    std::cout << "  ✓ Created synthetic sounds" << std::endl;
    
    // Initialize platform
    AudioToolboxDevice device;
    AudioToolboxFileDecoder decoder;
    MixerSession session(device, decoder);
    
    // Create generator types
    GeneratorType highClicks = {{clickHigh, clickMid}};
    GeneratorType lowClicks  = {{clickMid, clickLow}};
    GeneratorType bassClicks = {{bass}};
    
    // Build soundscape
    std::cout << "\nBuilding soundscape..." << std::endl;
    
    // Layer 1: High clicks with humanize + gain
    // (burst removed — TODO: reimplement as SignalTransform-based construct)
    Generator highGenerator(highClicks, {3, 1}); // 75% high, 25% mid
    auto highMorphism = Compose<AudioEvent>(
        humanize(0.01f, 0.1f),
        gain(0.6f)
    );
    AudioSource highPattern(AudioSource(highGenerator, decoder), highMorphism);
    
    // Layer 2: Low clicks with humanize
    // (project with varying signal removed — Project currently requires
    //  constant indices; random lane selection needs further design)
    Generator lowGenerator(lowClicks, {1, 3}); // 25% mid, 75% low
    AudioSource lowPattern(
        AudioSource(lowGenerator, decoder),
        humanize(0.02f, 0.15f)
    );
    
    // Layer 3: Bass hits with gain and random pitch variation
    // (burst removed — TODO: reimplement)
    Generator bassGenerator(bassClicks);
    auto bassMorphism = Compose<AudioEvent>(
        gain(0.8f),
        pitchMultiply(randomUniform(0.95f, 1.05f))
    );
    AudioSource bassPattern(AudioSource(bassGenerator, decoder), bassMorphism);
    
    std::cout << "  ✓ Created 3 soundscape layers" << std::endl;
    
    // Schedule layers
    std::cout << "\nScheduling layers:" << std::endl;
    std::cout << "  - High pattern: 0.5s period" << std::endl;
    std::cout << "  - Low pattern: 0.667s period (polyrhythm 3:2)" << std::endl;
    std::cout << "  - Bass pattern: 2.0s period" << std::endl;
    
    session.makeScheduler(highPattern, 0.5f, 0.01f);
    session.makeScheduler(lowPattern, 0.667f, 0.01f);
    session.makeScheduler(bassPattern, 2.0f, 0.01f);
    
    // Start playback
    std::cout << "\n▶ Starting soundscape (press Enter to stop)..." << std::endl;
    session.start();
    
    std::thread inputThread([&](){
        std::cin.get();
        session.stop();
    });
    
    inputThread.join();
    
    std::cout << "\n⏹ Stopped." << std::endl;
    std::cout << "\n=== Soundscape Test Complete ===\n" << std::endl;
    
    return 0;
}