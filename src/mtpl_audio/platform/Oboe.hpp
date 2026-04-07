#pragma once
// ============================================================================
//  Oboe platform implementation — Android
// ============================================================================

#include "mtpl_audio/audio/PlatformAudio.hpp"

#if defined(__ANDROID__)
#include <oboe/Oboe.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace mtpl {

// ----------------------------------------------------------------------------
//  OboeFileDecoder — uses Android NDK MediaCodec to decode audio files
// ----------------------------------------------------------------------------
class OboeFileDecoder : public AudioFileDecoder {
public:
    DecodedAudio decode(const std::string& path) override {
        AMediaExtractor* extractor = AMediaExtractor_new();
        media_status_t err = AMediaExtractor_setDataSource(extractor, path.c_str());
        if (err != AMEDIA_OK) {
            AMediaExtractor_delete(extractor);
            throw std::runtime_error("OboeFileDecoder: failed to open " + path);
        }

        // Find the audio track
        size_t trackCount = AMediaExtractor_getTrackCount(extractor);
        int audioTrack = -1;
        AMediaFormat* format = nullptr;
        for (size_t i = 0; i < trackCount; ++i) {
            format = AMediaExtractor_getTrackFormat(extractor, i);
            const char* mime = nullptr;
            AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
            if (mime && strncmp(mime, "audio/", 6) == 0) {
                audioTrack = (int)i;
                break;
            }
            AMediaFormat_delete(format);
            format = nullptr;
        }

        if (audioTrack < 0) {
            AMediaExtractor_delete(extractor);
            throw std::runtime_error("OboeFileDecoder: no audio track in " + path);
        }

        int32_t sampleRate = 44100, channels = 2;
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);

        AMediaExtractor_selectTrack(extractor, audioTrack);

        // Create decoder
        const char* mime = nullptr;
        AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
        AMediaCodec* codec = AMediaCodec_createDecoderByType(mime);
        AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
        AMediaCodec_start(codec);

        std::vector<float> samples;
        bool inputDone = false, outputDone = false;

        while (!outputDone) {
            // Feed input
            if (!inputDone) {
                ssize_t idx = AMediaCodec_dequeueInputBuffer(codec, 2000);
                if (idx >= 0) {
                    size_t bufSize = 0;
                    uint8_t* buf = AMediaCodec_getInputBuffer(codec, idx, &bufSize);
                    ssize_t read = AMediaExtractor_readSampleData(extractor, buf, bufSize);
                    if (read < 0) {
                        AMediaCodec_queueInputBuffer(codec, idx, 0, 0, 0,
                            AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                        inputDone = true;
                    } else {
                        int64_t pts = AMediaExtractor_getSampleTime(extractor);
                        AMediaCodec_queueInputBuffer(codec, idx, 0, read, pts, 0);
                        AMediaExtractor_advance(extractor);
                    }
                }
            }

            // Drain output
            AMediaCodecBufferInfo info;
            ssize_t idx = AMediaCodec_dequeueOutputBuffer(codec, &info, 2000);
            if (idx >= 0) {
                size_t outSize = 0;
                uint8_t* outBuf = AMediaCodec_getOutputBuffer(codec, idx, &outSize);
                // NDK MediaCodec outputs 16-bit PCM by default
                int16_t* pcm16 = reinterpret_cast<int16_t*>(outBuf + info.offset);
                size_t sampleCount = info.size / sizeof(int16_t);
                for (size_t i = 0; i < sampleCount; ++i) {
                    samples.push_back(pcm16[i] / 32768.0f);
                }
                AMediaCodec_releaseOutputBuffer(codec, idx, false);
                if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                    outputDone = true;
            }
        }

        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);

        return { samples, (double)sampleRate, (int)channels };
    }
};

// ----------------------------------------------------------------------------
//  OboeDevice — uses Google Oboe for low-latency audio output
// ----------------------------------------------------------------------------
class OboeDevice : public AudioDevice,
                   public oboe::AudioStreamDataCallback {
public:
    ~OboeDevice() override { stop(); }

    void start(FillCallback cb) override {
        callback_ = cb;

        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output)
               ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
               ->setSharingMode(oboe::SharingMode::Exclusive)
               ->setFormat(oboe::AudioFormat::Float)
               ->setChannelCount(2)
               ->setSampleRate(48000)
               ->setDataCallback(this);

        oboe::Result result = builder.openStream(stream_);
        if (result != oboe::Result::OK)
            throw std::runtime_error("OboeDevice: failed to open stream");

        result = stream_->requestStart();
        if (result != oboe::Result::OK)
            throw std::runtime_error("OboeDevice: failed to start stream");
    }

    void pause() override {
        if (stream_) stream_->requestPause();
    }

    void resume() override {
        if (stream_) stream_->requestStart();
    }

    void stop() override {
        if (stream_) {
            stream_->requestStop();
            stream_->close();
            stream_.reset();
        }
    }

    // oboe::AudioStreamDataCallback
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* /*stream*/,
        void* audioData,
        int32_t numFrames) override
    {
        callback_(static_cast<float*>(audioData), numFrames);
        return oboe::DataCallbackResult::Continue;
    }

private:
    FillCallback                       callback_;
    std::shared_ptr<oboe::AudioStream> stream_;
};

} // namespace mtpl

#else
// Stub: Oboe is only available on Android
#endif
