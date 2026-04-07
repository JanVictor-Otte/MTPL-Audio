#pragma once
// ============================================================================
//  ALSA platform implementation — Linux
// ============================================================================

#include "mtpl_audio/audio/PlatformAudio.hpp"

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <cstring>

namespace mtpl {

// ----------------------------------------------------------------------------
//  AlsaFileDecoder — uses libsndfile to decode audio files
// ----------------------------------------------------------------------------
class AlsaFileDecoder : public AudioFileDecoder {
public:
    DecodedAudio decode(const std::string& path) override {
        SF_INFO info = {};
        SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
        if (!file)
            throw std::runtime_error("AlsaFileDecoder: failed to open " +
                                     path + " — " + sf_strerror(nullptr));

        std::vector<float> samples(info.frames * info.channels);
        sf_count_t read = sf_readf_float(file, samples.data(), info.frames);
        sf_close(file);
        samples.resize(read * info.channels);

        return { samples, (double)info.samplerate, info.channels };
    }
};

// ----------------------------------------------------------------------------
//  AlsaDevice — uses ALSA PCM for audio output
// ----------------------------------------------------------------------------
class AlsaDevice : public AudioDevice {
public:
    static constexpr unsigned int kSampleRate   = 48000;
    static constexpr int          kChannels     = 2;
    static constexpr snd_pcm_uframes_t kPeriodFrames = 2048;

    ~AlsaDevice() override { stop(); }

    void start(FillCallback cb) override {
        callback_ = cb;

        int err = snd_pcm_open(&pcm_, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0)
            throw std::runtime_error(std::string("AlsaDevice: snd_pcm_open failed — ") +
                                     snd_strerror(err));

        snd_pcm_hw_params_t* params = nullptr;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(pcm_, params);
        snd_pcm_hw_params_set_access(pcm_, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcm_, params, SND_PCM_FORMAT_FLOAT_LE);

        unsigned int rate = kSampleRate;
        snd_pcm_hw_params_set_rate_near(pcm_, params, &rate, nullptr);
        snd_pcm_hw_params_set_channels(pcm_, params, kChannels);

        snd_pcm_uframes_t period = kPeriodFrames;
        snd_pcm_hw_params_set_period_size_near(pcm_, params, &period, nullptr);

        err = snd_pcm_hw_params(pcm_, params);
        if (err < 0)
            throw std::runtime_error(std::string("AlsaDevice: hw_params failed — ") +
                                     snd_strerror(err));

        snd_pcm_prepare(pcm_);
        running_ = true;
        renderThread_ = std::thread([this] { renderLoop(); });
    }

    void pause() override {
        if (pcm_) snd_pcm_pause(pcm_, 1);
    }

    void resume() override {
        if (pcm_) snd_pcm_pause(pcm_, 0);
    }

    void stop() override {
        running_ = false;
        if (renderThread_.joinable()) renderThread_.join();
        if (pcm_) {
            snd_pcm_drop(pcm_);
            snd_pcm_close(pcm_);
            pcm_ = nullptr;
        }
    }

private:
    FillCallback      callback_;
    std::atomic<bool> running_{false};
    snd_pcm_t*        pcm_ = nullptr;
    std::thread       renderThread_;

    void renderLoop() {
        std::vector<float> buffer(kPeriodFrames * kChannels);
        while (running_) {
            std::memset(buffer.data(), 0, buffer.size() * sizeof(float));
            callback_(buffer.data(), (int)kPeriodFrames);

            snd_pcm_sframes_t written = snd_pcm_writei(pcm_, buffer.data(), kPeriodFrames);
            if (written < 0) {
                snd_pcm_recover(pcm_, (int)written, /*silent=*/1);
            }
        }
    }
};

} // namespace mtpl

#else
// Stub: ALSA is only available on Linux
#endif
