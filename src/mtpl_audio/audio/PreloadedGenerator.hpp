#pragma once
#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/audio/Generator.hpp"
#include "mtpl_audio/audio/SoundBuffer.hpp"
#include <vector>
#include <stdexcept>

namespace mtpl {

// ============================================================================
//  Layer 3: PreloadedGenerator — combines Generator spec with loaded audio
//  
//  This is where Generator (specification) meets SoundBuffer (runtime data).
//  Lives in its own file to avoid circular dependencies.
// ============================================================================

struct PreloadedGenerator : PreloadedGeneratorBase {
    Generator                generator;
    std::vector<SoundBuffer> buffers;
    std::vector<float>       attackTimes;

    PreloadedGenerator(Generator g, AudioFileDecoder& decoder,
                       std::vector<float> attacks = {})
        : generator(std::move(g))
    {
        for (const auto& path : generator.type.soundFiles)
            buffers.push_back(SoundBuffer::load(path, decoder, generator.quality));

        // Explicit attacks > Generator's attackTimes > zeros
        if (!attacks.empty())
            attackTimes = std::move(attacks);
        else if (!generator.attackTimes.empty())
            attackTimes = generator.attackTimes;
        else
            attackTimes.resize(buffers.size(), 0.f);
        if (attackTimes.size() != buffers.size())
            throw std::runtime_error("PreloadedGenerator: attackTimes size mismatch");
    }
    
    SampledVariant sample() const override {
        int i = generator.sample();
        return { &buffers[i], attackTimes[i] };
    }
};

} // namespace mtpl