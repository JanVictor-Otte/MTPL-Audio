#pragma once

#include "mtpl_audio/audio/PlatformAudio.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#include "mtpl_audio/audio/ResampleQuality.hpp"

namespace mtpl {
// ============================================================================
//  Constants
// ============================================================================

static constexpr double kOutputSampleRate = 48000.0;
static constexpr int    kOutputChannels   = 2;
static constexpr int    kSincTaps         = 64;
static constexpr double kKaiserBeta       = 8.0;

// ============================================================================
//  Fast linear-interpolation resampler — ~100x faster than sinc for short clips
// ============================================================================

static std::vector<float> linearResample(
    const std::vector<float>& src, double srcRate)
{
    if (std::abs(srcRate - kOutputSampleRate) < 1.0) return src;

    double ratio    = kOutputSampleRate / srcRate;
    int    outLen   = (int)std::ceil(src.size() * ratio);
    std::vector<float> out(outLen);

    for (int i = 0; i < outLen; ++i) {
        double pos = i / ratio;
        int    i0  = (int)pos;
        int    i1  = std::min(i0 + 1, (int)src.size() - 1);
        double frac = pos - i0;
        out[i] = (float)(src[i0] * (1.0 - frac) + src[i1] * frac);
    }
    return out;
}

// ============================================================================
//  Kaiser window and windowed-sinc resampler — pure math, no platform deps
// ============================================================================

static double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k <= 30; ++k) {
        term *= (x / 2.0) / k;
        sum  += term * term;
    }
    return sum;
}

static double kaiserWindow(int n, int numTaps, double beta) {
    double half  = (numTaps - 1) / 2.0;
    double ratio = (n - half) / half;
    return besselI0(beta * std::sqrt(1.0 - ratio * ratio)) / besselI0(beta);
}

static double sincFunc(double x) {
    if (std::abs(x) < 1e-9) return 1.0;
    return std::sin(M_PI * x) / (M_PI * x);
}

static std::vector<float> sincResample(
    const std::vector<float>& src, double srcRate)
{
    if (std::abs(srcRate - kOutputSampleRate) < 1.0) return src;

    double ratio     = kOutputSampleRate / srcRate;
    double cutoff    = std::min(1.0, ratio);
    int    outFrames = (int)std::ceil(src.size() * ratio);
    std::vector<float> out(outFrames, 0.f);

    for (int i = 0; i < outFrames; ++i) {
        double srcPos = i / ratio;
        double sum = 0.0, norm = 0.0;
        int    start = (int)std::floor(srcPos) - kSincTaps / 2 + 1;
        int    end   = start + kSincTaps;
        for (int j = start; j < end; ++j) {
            if (j < 0 || j >= (int)src.size()) continue;
            double x   = (srcPos - j) * cutoff;
            int    tap = j - start;
            double w   = kaiserWindow(tap, kSincTaps, kKaiserBeta);
            double h   = sincFunc(x) * cutoff * w;
            sum  += src[j] * h;
            norm += h;
        }
        out[i] = (float)(norm > 1e-9 ? sum / norm : 0.0);
    }
    return out;
}

// ============================================================================
//  SoundBuffer
//
//  Constructed from DecodedAudio (platform-provided raw PCM).
//  Resamples to kOutputSampleRate and converts to stereo interleaved float.
//  After construction: samples = [L0,R0,L1,R1,...] at 48kHz stereo.
//
//  Convenience factory: SoundBuffer::load(path, decoder)
// ============================================================================

struct SoundBuffer {
    std::vector<float> samples;   // stereo interleaved, 48kHz
    int                numFrames = 0;

    // Construct from already-decoded audio
    explicit SoundBuffer(const DecodedAudio& decoded,
                         ResampleQuality quality = ResampleQuality::Linear) {
        int    srcChannels = decoded.channels;
        double srcRate     = decoded.sampleRate;
        const auto& src    = decoded.samples;
        int    srcFrames   = (int)src.size() / srcChannels;

        auto resample = (quality == ResampleQuality::Sinc)
            ? sincResample : linearResample;

        // Split into per-channel buffers, resample each to 48kHz
        std::vector<float> ch0(srcFrames), ch1(srcFrames);
        for (int i = 0; i < srcFrames; ++i) {
            ch0[i] = src[i * srcChannels + 0];
            ch1[i] = (srcChannels > 1) ? src[i * srcChannels + 1] : ch0[i];
        }
        auto rs0 = resample(ch0, srcRate);
        auto rs1 = resample(ch1, srcRate);

        numFrames = (int)std::min(rs0.size(), rs1.size());
        samples.resize(numFrames * 2);
        for (int i = 0; i < numFrames; ++i) {
            samples[i * 2 + 0] = rs0[i];
            samples[i * 2 + 1] = rs1[i];
        }
    }

    // Convenience: decode via platform decoder then construct
    static SoundBuffer load(const std::string& path, AudioFileDecoder& decoder,
                            ResampleQuality quality = ResampleQuality::Linear) {
        return SoundBuffer(decoder.decode(path), quality);
    }
};
}