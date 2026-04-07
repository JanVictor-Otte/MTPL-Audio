#pragma once
#include "mtpl/core/Source.hpp"

#include "mtpl_audio/audio/PreloadedGenerator.hpp"
#include <memory>
#include <vector>
#include <stdexcept>

namespace mtpl {

// ============================================================================
//  AudioSource — audio-specific Source with Generator/oneshot constructors
// ============================================================================

class AudioSource : public Source<AudioEvent> {
public:
    // Inherit base constructor (for custom sources)
    using Source<AudioEvent>::Source;
    
    // ========================================================================
    // From PreloadedGenerator
    // ========================================================================
    
    explicit AudioSource(std::shared_ptr<PreloadedGenerator> base)
        : Source<AudioEvent>(PrimitiveSource<AudioEvent>(1, [base]() {
            return AudioMultiLane{
                AudioLane{
                    AudioEvent{0.0f, AudioPayload{base, 1.0f, 1.0f}}
                }
            };
        })) {}

    // From PreloadedGenerator value (wraps in shared_ptr)
    explicit AudioSource(PreloadedGenerator pg)
        : AudioSource(std::make_shared<PreloadedGenerator>(std::move(pg))) {}
    
    // ========================================================================
    // From Source<AudioEvent>
    // ========================================================================
    
    AudioSource(Source<AudioEvent> src)
        : Source<AudioEvent>(std::move(src)) {}

    // ========================================================================
    // From Generator + decoder (high-level convenience)
    // Quality is read from Generator.quality (defaults to Linear)
    // ========================================================================
    
    AudioSource(Generator generator, AudioFileDecoder& decoder, 
                std::vector<float> attackTimes = {})
        : AudioSource(std::make_shared<PreloadedGenerator>(
            std::move(generator), decoder, std::move(attackTimes))) {}
    
    AudioSource(GeneratorType type, AudioFileDecoder& decoder,
                std::vector<float> attackTimes = {})
        : AudioSource(Generator(std::move(type)), decoder, std::move(attackTimes)) {}
    
    AudioSource(const std::string& soundFile, AudioFileDecoder& decoder,
                float attackTime = 0.0f,
                ResampleQuality quality = ResampleQuality::Linear)
        : AudioSource(Generator(soundFile, 1.f, attackTime, quality), decoder) {}

    // Convenience: skip attack time, just specify quality
    AudioSource(const std::string& soundFile, AudioFileDecoder& decoder,
                ResampleQuality quality)
        : AudioSource(soundFile, decoder, 0.0f, quality) {}
    
    // ========================================================================
    // Static factories
    // ========================================================================
    
    // OneShot: single event at offset time (from PreloadedGenerator)
    static AudioSource oneShot(std::shared_ptr<PreloadedGenerator> base, 
                               float offsetSeconds) {
        return AudioSource(Source<AudioEvent>(PrimitiveSource<AudioEvent>(1, [base, offsetSeconds]() {
            return AudioMultiLane{
                AudioLane{
                    AudioEvent{offsetSeconds, AudioPayload{base, 1.0f, 1.0f}}
                }
            };
        })));
    }
    
    // OneShot: from Generator + decoder
    static AudioSource oneShot(Generator Generator, AudioFileDecoder& decoder,
                               float offsetSeconds,
                               std::vector<float> attackTimes = {}) {
        auto base = std::make_shared<PreloadedGenerator>(
            std::move(Generator), decoder, std::move(attackTimes));
        return oneShot(base, offsetSeconds);
    }
    
    // Silence: empty source
    static AudioSource silence() {
        return AudioSource(Source<AudioEvent>(PrimitiveSource<AudioEvent>(1, []() {
            return AudioMultiLane{AudioLane{}};
        })));
    }
};

} // namespace mtpl