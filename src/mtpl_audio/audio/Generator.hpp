#pragma once
#include "mtpl_audio/audio/ResampleQuality.hpp"
#include <vector>
#include <string>
#include <random>
#include <numeric>
#include <stdexcept>

namespace mtpl {

struct GeneratorType { std::vector<std::string> soundFiles; };

struct Generator {
    GeneratorType        type;
    std::vector<float>   attackTimes;
    std::vector<float>   weights;
    ResampleQuality      quality = ResampleQuality::Linear;

    Generator(GeneratorType t, std::vector<float> w,
              ResampleQuality q = ResampleQuality::Linear)
        : type(std::move(t)), weights(std::move(w)), quality(q) {
        if (type.soundFiles.size() != weights.size())
            throw std::runtime_error("Generator: soundFiles/weights mismatch");
    }
    Generator(GeneratorType t, std::vector<float> attacks, std::vector<float> w,
              ResampleQuality q = ResampleQuality::Linear)
        : type(std::move(t)), weights(std::move(w)),
          attackTimes(std::move(attacks)), quality(q) {
        if (type.soundFiles.size() != weights.size())
            throw std::runtime_error("Generator: soundFiles/weights mismatch");
        if (!attackTimes.empty() && attackTimes.size() != type.soundFiles.size())
            throw std::runtime_error("Generator: soundFiles/attackTimes mismatch");
    }
    explicit Generator(GeneratorType t,
                       ResampleQuality q = ResampleQuality::Linear)
        : type(std::move(t)), weights(type.soundFiles.size(), 1.f), quality(q) {}
    explicit Generator(const std::string& f, float weight = 1.f,
                       float attackTime = 0.f,
                       ResampleQuality q = ResampleQuality::Linear)
        : type(GeneratorType{{f}}), weights({weight}),
          attackTimes({attackTime}), quality(q) {}

    int sample() const {
        static std::mt19937 rng(std::random_device{}());
        float total = std::accumulate(weights.begin(), weights.end(), 0.f);
        float roll  = std::uniform_real_distribution<float>(0.f, total)(rng);
        float cum   = 0.f;
        for (size_t i = 0; i < weights.size(); ++i) {
            cum += weights[i];
            if (roll < cum) return (int)i;
        }
        return (int)weights.size() - 1;
    }
};

}