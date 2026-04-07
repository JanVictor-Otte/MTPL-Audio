#pragma once
// ============================================================================
//  WASAPI platform implementation — Windows
// ============================================================================

#include "mtpl_audio/audio/PlatformAudio.hpp"

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace mtpl {

// ----------------------------------------------------------------------------
//  WasapiFileDecoder — uses Media Foundation Source Reader to decode audio
// ----------------------------------------------------------------------------
class WasapiFileDecoder : public AudioFileDecoder {
public:
    WasapiFileDecoder() {
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
            throw std::runtime_error("WasapiFileDecoder: MFStartup failed");
    }

    ~WasapiFileDecoder() { MFShutdown(); }

    DecodedAudio decode(const std::string& path) override {
        // Convert path to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

        IMFSourceReader* reader = nullptr;
        HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
        if (FAILED(hr))
            throw std::runtime_error("WasapiFileDecoder: failed to open " + path);

        // Request decoded PCM float output
        IMFMediaType* outputType = nullptr;
        MFCreateMediaType(&outputType);
        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                    nullptr, outputType);
        outputType->Release();

        // Read actual output format to get sample rate and channels
        IMFMediaType* actualType = nullptr;
        reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actualType);
        UINT32 channels = 0, sampleRate = 0;
        actualType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        actualType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
        actualType->Release();

        // Read all samples
        std::vector<float> samples;
        for (;;) {
            DWORD flags = 0;
            IMFSample* sample = nullptr;
            hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                    0, nullptr, &flags, nullptr, &sample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                if (sample) sample->Release();
                break;
            }
            if (sample) {
                IMFMediaBuffer* buf = nullptr;
                sample->ConvertToContiguousBuffer(&buf);
                BYTE* data = nullptr;
                DWORD len = 0;
                buf->Lock(&data, nullptr, &len);
                size_t floatCount = len / sizeof(float);
                const float* fdata = reinterpret_cast<const float*>(data);
                samples.insert(samples.end(), fdata, fdata + floatCount);
                buf->Unlock();
                buf->Release();
                sample->Release();
            }
        }
        reader->Release();

        return { samples, (double)sampleRate, (int)channels };
    }
};

// ----------------------------------------------------------------------------
//  WasapiDevice — uses WASAPI shared-mode for audio output
// ----------------------------------------------------------------------------
class WasapiDevice : public AudioDevice {
public:
    ~WasapiDevice() override { stop(); }

    void start(FillCallback cb) override {
        callback_ = cb;
        running_ = true;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != S_FALSE)
            throw std::runtime_error("WasapiDevice: CoInitializeEx failed");

        IMMDeviceEnumerator* enumerator = nullptr;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                         CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                         (void**)&enumerator);

        IMMDevice* device = nullptr;
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        enumerator->Release();

        device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                         nullptr, (void**)&audioClient_);
        device->Release();

        // Set up format: stereo float 48kHz
        WAVEFORMATEX fmt = {};
        fmt.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        fmt.nChannels       = 2;
        fmt.nSamplesPerSec  = 48000;
        fmt.wBitsPerSample  = 32;
        fmt.nBlockAlign     = fmt.nChannels * fmt.wBitsPerSample / 8;
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

        REFERENCE_TIME bufDur = 20 * 10000; // 20ms buffer
        audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  0, bufDur, 0, &fmt, nullptr);

        audioClient_->GetBufferSize(&bufferFrames_);
        audioClient_->GetService(__uuidof(IAudioRenderClient),
                                  (void**)&renderClient_);

        audioClient_->Start();

        renderThread_ = std::thread([this] { renderLoop(); });
    }

    void pause() override {
        if (audioClient_) audioClient_->Stop();
    }

    void resume() override {
        if (audioClient_) audioClient_->Start();
    }

    void stop() override {
        running_ = false;
        if (renderThread_.joinable()) renderThread_.join();
        if (renderClient_) { renderClient_->Release(); renderClient_ = nullptr; }
        if (audioClient_)  { audioClient_->Stop(); audioClient_->Release(); audioClient_ = nullptr; }
        CoUninitialize();
    }

private:
    FillCallback         callback_;
    std::atomic<bool>    running_{false};
    IAudioClient*        audioClient_  = nullptr;
    IAudioRenderClient*  renderClient_ = nullptr;
    UINT32               bufferFrames_ = 0;
    std::thread          renderThread_;

    void renderLoop() {
        while (running_) {
            UINT32 padding = 0;
            audioClient_->GetCurrentPadding(&padding);
            UINT32 available = bufferFrames_ - padding;
            if (available > 0) {
                BYTE* data = nullptr;
                renderClient_->GetBuffer(available, &data);
                callback_(reinterpret_cast<float*>(data), (int)available);
                renderClient_->ReleaseBuffer(available, 0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};

} // namespace mtpl

#else
// Stub: WASAPI is only available on Windows
#endif
