#pragma once

#include "mtpl_audio/core/AudioSource.hpp"
#include "mtpl_audio/audio/Mixer.hpp"

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace mtpl {

// ============================================================================
//  LoopScheduler
// ============================================================================

class LoopScheduler {
public:
    LoopScheduler(AudioSource source, float periodSeconds, float maxJitterSeconds,
                  Mixer& mixer,
                  const std::chrono::steady_clock::time_point& startTime)
        : source_(source)
        , period_(periodSeconds)
        , maxJitter_(maxJitterSeconds)
        , running_(false)
        , mixer_(mixer)
        , startTime_(startTime)
    {}

    void start() {
        running_ = true;
        thread_  = std::thread([this]{ run(); });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    // Live update — takes effect at the start of the next loop
    void setSource(AudioSource newSource) {
        std::lock_guard<std::mutex> lock(sourceMutex_);
        pendingSource_ = newSource;
        sourceUpdated_ = true;
    }

    ~LoopScheduler() { stop(); }

private:
    AudioSource       source_;
    float             period_;
    float             maxJitter_;
    std::atomic<bool> running_;
    std::thread       thread_;
    Mixer&            mixer_;
    const std::chrono::steady_clock::time_point& startTime_;
    std::mutex        sourceMutex_;
    AudioSource       pendingSource_ = AudioSource::silence();
    std::atomic<bool> sourceUpdated_ = false;

    double now() const {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - startTime_).count();
    }

    void scheduleLoop(int loopIndex) {
        AudioLane events  = evaluate(source_, period_, 1);
        double loopOffset = loopIndex * (double)period_;

        for (const auto& e : events) {
            if (!e.payload.generator) continue;
            auto* pc = static_cast<PreloadedGenerator*>(e.payload.generator.get());
            auto  sv = pc->sample();

            double abstractTime = loopOffset + e.time;
            // Adjust attack time for pitch: higher pitch → faster playback → shorter attack
            double fireTime     = abstractTime - sv.attackTime / (double)e.payload.pitch;

            double lookahead = 0.005;
            while (now() < fireTime - lookahead && running_)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

            if (!running_) return;
            mixer_.schedule(sv.buffer, e.payload.pitch, e.payload.gain, fireTime);
        }
    }

    void run() {
        int loop = 0;
        while (running_) {
            if (sourceUpdated_) {
                std::lock_guard<std::mutex> lock(sourceMutex_);
                source_        = pendingSource_;
                sourceUpdated_ = false;
            }
            scheduleLoop(loop);

            double nextLoopStart = (loop + 1) * (double)period_;
            double waitUntil     = nextLoopStart - (double)maxJitter_;
            while (now() < waitUntil && running_)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

            ++loop;
        }
    }
};

// ============================================================================
//  MixerSession
//
//  Owns the Mixer and startTime. Takes AudioDevice& and AudioFileDecoder&
//  from the platform — the library never sees platform-specific types.
//
//  Usage:
//      AudioToolboxDevice      device;
//      AudioToolboxFileDecoder decoder;
//      MixerSession session(device, decoder);
//
//      auto src = Generator(myGenerator, session.decoder());
//      session.makeScheduler(src, 1.0f, 0.01f);
//      session.start();
// ============================================================================

class MixerSession {
public:
    MixerSession(AudioDevice& device, AudioFileDecoder& decoder)
        : device_(device), decoder_(decoder) {}

    MixerSession(const MixerSession&)            = delete;
    MixerSession& operator=(const MixerSession&) = delete;

    // Expose decoder so call sites can use session.decoder() as a convenience
    AudioFileDecoder& decoder() { return decoder_; }

    LoopScheduler& makeScheduler(AudioSource source, float period, float maxJitter) {
        schedulers_.push_back(std::make_unique<LoopScheduler>(
            source, period, maxJitter, mixer_, startTime_));
        return *schedulers_.back();
    }

    void start() {
        startTime_ = std::chrono::steady_clock::now();
        mixer_.reset();
        device_.start([this](float* buf, int n){ mixer_.fill(buf, n); });
        for (auto& s : schedulers_) s->start();
    }

    void pause() {
        for (auto& s : schedulers_) s->stop();
        device_.pause();
    }

    void resume() {
        startTime_ = std::chrono::steady_clock::now();
        device_.resume();
        for (auto& s : schedulers_) s->start();
    }

    void stop() {
        for (auto& s : schedulers_) s->stop();
        device_.stop();
    }

    void updateSource(int schedulerIndex, AudioSource newSource) {
        schedulers_[schedulerIndex]->setSource(newSource);
    }

    ~MixerSession() { stop(); }

private:
    AudioDevice&      device_;
    AudioFileDecoder& decoder_;
    Mixer             mixer_;
    std::chrono::steady_clock::time_point startTime_;
    std::vector<std::unique_ptr<LoopScheduler>> schedulers_;
};
}