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
#include <filesystem>

using namespace mtpl;
namespace fs = std::filesystem;

// Resolve a sound path relative to the project root
std::string soundPath(const std::string& relative) {
    // Look for soundfiles/ relative to the executable's grandparent (build/../)
    fs::path exe = fs::current_path();
    fs::path candidate = exe / ".." / relative;
    if (fs::exists(candidate)) return fs::canonical(candidate).string();
    // Also try from the build dir directly
    candidate = exe / relative;
    if (fs::exists(candidate)) return fs::canonical(candidate).string();
    throw std::runtime_error("Sound file not found: " + relative);
}

int main() {
    std::cout << "\n=== MTPL-Audio Clickscape Test ===\n" << std::endl;

    // ================================================================
    //  Platform
    // ================================================================
    AudioToolboxDevice device;
    AudioToolboxFileDecoder decoder;
    MixerSession session(device, decoder);

    // ================================================================
    //  Load real sound files from soundfiles/
    // ================================================================
    std::cout << "Loading sounds..." << std::endl;

    // Pen clicks — on/off pairs from two pen variations
    std::string pen1_on  = soundPath("soundfiles/pen_1/variation_1/on.wav");
    std::string pen1_off = soundPath("soundfiles/pen_1/variation_1/off.wav");
    std::string pen2_on  = soundPath("soundfiles/pen_2/variation_1/on.wav");
    std::string pen2_off = soundPath("soundfiles/pen_2/variation_1/off.wav");
    std::string pen2v2_on  = soundPath("soundfiles/pen_2/variation_2/on.wav");
    std::string pen2v2_off = soundPath("soundfiles/pen_2/variation_2/off.wav");

    // Dab clicks
    std::string dab1 = soundPath("soundfiles/dab_1/var_1.wav");
    std::string dab2 = soundPath("soundfiles/dab_1/var_2.wav");
    std::string dab3 = soundPath("soundfiles/dab_1/var_3.wav");
    std::string dab4 = soundPath("soundfiles/dab_1/var_4.wav");

    // Hollow taps
    std::string hollow1 = soundPath("soundfiles/hollow_1/var_1.wav");
    std::string hollow2 = soundPath("soundfiles/hollow_2/var_1.wav");

    // Hollow tube
    std::string tube1 = soundPath("soundfiles/hollow_tube/var_1.wav");
    std::string tube2 = soundPath("soundfiles/hollow_tube/var_2.wav");
    std::string tube3 = soundPath("soundfiles/hollow_tube/var_3.wav");
    std::string tube4 = soundPath("soundfiles/hollow_tube/var_4.wav");

    // T sound
    std::string t1 = soundPath("soundfiles/t_sound/var_1.wav");
    std::string t2 = soundPath("soundfiles/t_sound/var_2.wav");
    std::string t3 = soundPath("soundfiles/t_sound/var_3.wav");
    std::string t4 = soundPath("soundfiles/t_sound/var_4.wav");

    // Knock
    std::string knock = soundPath("soundfiles/knock/var_1.wav");

    std::cout << "  ✓ All sound files located" << std::endl;

    // ================================================================
    //  Generator types — weighted random sound selection
    // ================================================================
    
    // Pen clicks: mix of on/off from different pens
    GeneratorType penClicks = {{pen1_on, pen1_off, pen2_on, pen2_off, pen2v2_on, pen2v2_off}};
    Generator penGen(penClicks, {2, 2, 1, 1, 1, 1}); // favour pen_1

    // Dab clicks: 4 variations equally weighted
    GeneratorType dabClicks = {{dab1, dab2, dab3, dab4}};
    Generator dabGen(dabClicks);

    // Hollow taps: 2 types
    GeneratorType hollowTaps = {{hollow1, hollow2}};
    Generator hollowGen(hollowTaps);

    // Tube resonances: 4 variations
    GeneratorType tubeSounds = {{tube1, tube2, tube3, tube4}};
    Generator tubeGen(tubeSounds);

    // T sounds: 4 variations
    GeneratorType tSounds({{t1, t2, t3, t4}});
    std::vector<float> tAttackTimes = {0.047f, 0.024f, 0.038f, 0.032f}; // staggered attack times for more texture
    Generator tGen(tSounds, tAttackTimes, std::vector<float>{1.f, 1.f, 1.f, 1.f});

    // Knock: 1 variation
    GeneratorType knockGenT({{knock}});
    Generator knockGen(knockGenT, {0.01f}, {1.f}, ResampleQuality::Linear); // add a tiny attack time to prevent clicks


    std::cout << "  ✓ Generators configured" << std::endl;

    // ================================================================
    //  Layer 1: Base layer. Rhythmic pen clicks
    // ================================================================
    AudioSource penPattern(
        AudioSource(tGen, decoder),
        Compose<AudioEvent>(
            gain(3.0f),
            Identity<AudioEvent>()
        )
    );

    std::cout << "  ✓ Layer 1 configured" << std::endl;

    // ================================================================
    //  Layer 2: Rolls of hollow2 sounds
    // ================================================================
    AudioSource hollowRoll(
        AudioSource(hollow2, decoder),
        Compose<AudioEvent>(
            humanize(0.005f, 0.05f, 5.0f),
            pitch(1.0f),   // down 2 semitones
            gain(0.6f)
        )
    );

    std::cout << "  ✓ Layer 2 configured" << std::endl;

    std::cout << "  ✓ 2 clickscape layers built" << std::endl;

    AudioSource tSource(AudioSource(knockGen, decoder),
        Compose<AudioEvent>(ntole(8, 0.2f), pitch(0.5*evaluationCount()) ));
    // ================================================================
    //  layering
    // ================================================================

    AudioSource combined = AudioSource(merge<AudioEvent>(hollowRoll, penPattern), Project<AudioEvent>(everyNth(2)));

    std::cout << "\nScheduling layers:" << std::endl;
    std::cout << "  - Pen rolls:     0.5s period  (fast, primary)" << std::endl;
    std::cout << "  - Dab doubles:   0.5s period  (3:2 against pens)" << std::endl;

    session.makeScheduler(tSource,    2.0f,  0.005f);

    // ================================================================
    //  Play
    // ================================================================
    std::cout << "\n▶ Starting clickscape (press Enter to stop)..." << std::endl;
    session.start();

    std::thread inputThread([&](){
        std::cin.get();
        session.stop();
    });

    inputThread.join();

    std::cout << "\n⏹ Stopped." << std::endl;
    std::cout << "\n=== Clickscape Test Complete ===\n" << std::endl;

    return 0;
}
