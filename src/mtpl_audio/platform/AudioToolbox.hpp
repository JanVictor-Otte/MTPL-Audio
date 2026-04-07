#pragma once
// ============================================================================
//  AudioToolbox platform implementation — macOS and iOS
// ============================================================================

#include "mtpl_audio/audio/PlatformAudio.hpp"

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <stdexcept>
#include <string>
#include <cstring>

namespace mtpl {
class AudioToolboxFileDecoder : public AudioFileDecoder {
public:
    DecodedAudio decode(const std::string& path) override {
        CFStringRef cfPath = CFStringCreateWithCString(
            kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
        CFURLRef url = CFURLCreateWithFileSystemPath(
            kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
        CFRelease(cfPath);

        ExtAudioFileRef extFile = nullptr;
        OSStatus err = ExtAudioFileOpenURL(url, &extFile);
        CFRelease(url);
        if (err != noErr)
            throw std::runtime_error("AudioToolboxFileDecoder: failed to open " +
                                     path + " (" + std::to_string(err) + ")");

        AudioStreamBasicDescription srcFmt = {};
        UInt32 size = sizeof(srcFmt);
        ExtAudioFileGetProperty(extFile,
            kExtAudioFileProperty_FileDataFormat, &size, &srcFmt);

        AudioStreamBasicDescription outFmt = {};
        outFmt.mSampleRate       = srcFmt.mSampleRate;
        outFmt.mFormatID         = kAudioFormatLinearPCM;
        outFmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        outFmt.mBitsPerChannel   = 32;
        outFmt.mChannelsPerFrame = srcFmt.mChannelsPerFrame;
        outFmt.mBytesPerFrame    = 4 * srcFmt.mChannelsPerFrame;
        outFmt.mFramesPerPacket  = 1;
        outFmt.mBytesPerPacket   = outFmt.mBytesPerFrame;
        ExtAudioFileSetProperty(extFile,
            kExtAudioFileProperty_ClientDataFormat, sizeof(outFmt), &outFmt);

        SInt64 numFrames = 0;
        size = sizeof(numFrames);
        ExtAudioFileGetProperty(extFile,
            kExtAudioFileProperty_FileLengthFrames, &size, &numFrames);

        int ch = (int)srcFmt.mChannelsPerFrame;
        std::vector<float> samples((size_t)(numFrames * ch));

        AudioBufferList abl;
        abl.mNumberBuffers              = 1;
        abl.mBuffers[0].mNumberChannels = ch;
        abl.mBuffers[0].mDataByteSize   = (UInt32)(samples.size() * sizeof(float));
        abl.mBuffers[0].mData           = samples.data();

        UInt32 framesToRead = (UInt32)numFrames;
        ExtAudioFileRead(extFile, &framesToRead, &abl);
        ExtAudioFileDispose(extFile);
        samples.resize((size_t)(framesToRead * ch));

        return { samples, srcFmt.mSampleRate, ch };
    }
};

class AudioToolboxDevice : public AudioDevice {
public:
    static constexpr int kBufferFrames = 2048;
    static constexpr int kNumBuffers   = 3;

    ~AudioToolboxDevice() override { stop(); }

    void start(FillCallback cb) override {
        callback_ = cb;

        AudioStreamBasicDescription fmt = {};
        fmt.mSampleRate       = 48000.0;
        fmt.mFormatID         = kAudioFormatLinearPCM;
        fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        fmt.mBitsPerChannel   = 32;
        fmt.mChannelsPerFrame = 2;
        fmt.mBytesPerFrame    = 8;
        fmt.mFramesPerPacket  = 1;
        fmt.mBytesPerPacket   = 8;

        OSStatus err = AudioQueueNewOutput(&fmt, audioQueueCallback, this,
                                           nullptr, nullptr, 0, &queue_);
        if (err != noErr)
            throw std::runtime_error("AudioToolboxDevice: AudioQueueNewOutput failed");

        UInt32 bufBytes = kBufferFrames * 8;
        for (int i = 0; i < kNumBuffers; ++i) {
            AudioQueueAllocateBuffer(queue_, bufBytes, &buffers_[i]);
            memset(buffers_[i]->mAudioData, 0, bufBytes);
            buffers_[i]->mAudioDataByteSize = bufBytes;
            AudioQueueEnqueueBuffer(queue_, buffers_[i], 0, nullptr);
        }
        AudioQueueStart(queue_, nullptr);
    }

    void pause() override  { if (queue_) AudioQueuePause(queue_); }
    void resume() override { if (queue_) AudioQueueStart(queue_, nullptr); }

    void stop() override {
        if (queue_) {
            AudioQueueStop(queue_, true);
            AudioQueueDispose(queue_, true);
            queue_ = nullptr;
        }
    }

private:
    FillCallback        callback_;
    AudioQueueRef       queue_ = nullptr;
    AudioQueueBufferRef buffers_[kNumBuffers] = {};

    static void audioQueueCallback(void* ud, AudioQueueRef aq, AudioQueueBufferRef buf) {
        auto* self = static_cast<AudioToolboxDevice*>(ud);
        buf->mAudioDataByteSize = kBufferFrames * 8;
        self->callback_((float*)buf->mAudioData, kBufferFrames);
        AudioQueueEnqueueBuffer(aq, buf, 0, nullptr);
    }
};
} // namespace mtpl

#else
// Stub: AudioToolbox is only available on Apple platforms
#endif