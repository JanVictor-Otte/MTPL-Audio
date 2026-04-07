#pragma once
#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/core/AudioSignal.hpp"
#include "mtpl_audio/audio/Dynamics.hpp"
#include "mtpl_audio/audio/Pitch.hpp"
#include <random>
#include <stdexcept>

namespace mtpl {

// burst — lane-expansion: replicate each event n times with interval offsets.
// This is a lane-level operation (changes event count), not a per-event transform.
// TODO: implement as a SignalTransform-based construct once the pattern
//       for lane-expanding morphisms with per-event signal evaluation is settled.

// timeOffset — per-event: shift event time by amount
inline EventLeaf<AudioEvent, float> timeOffset(AudioSignal<float> amount) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float a) -> AudioEvent {
            e.time += a;
            return e;
        },
        amount
    );
}
inline EventLeaf<AudioEvent, float> timeOffset(float v) { return timeOffset(constant(v)); }

// jitter — per-event: random time offset within ±maxSeconds
inline EventLeaf<AudioEvent, float> jitter(AudioSignal<float> maxSeconds) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float m) -> AudioEvent {
            static std::mt19937 rng(std::random_device{}());
            e.time += std::uniform_real_distribution<float>(-m, m)(rng);
            return e;
        },
        maxSeconds
    );
}
inline EventLeaf<AudioEvent, float> jitter(float v) { return jitter(constant(v)); }

// humanize — composition of time jitter, gain variance, pitch variance
inline Compose<AudioEvent> humanize(float timingSeconds,
                                     float gainVariance = 0.05f,
                                     float pitchVariance = 0.5f) {
    return Compose<AudioEvent>(
        timeOffset(randomUniform(-timingSeconds, timingSeconds)),
        gain(randomUniform(1.f - gainVariance, 1.f + gainVariance)),
        pitch(randomUniform(-pitchVariance, pitchVariance))
    );
}



// burst — lane-level: replicate each event n times with interval offsets.
//
// For each lane, for each event e:
//   1. Freeze count and interval signals
//   2. Sample count(e) → n for this particular event
//   3. Push e itself to the new lane
//   4. For i in 1..n-1:
//        a. Sample interval(e) → dt
//        b. Copy the previous replica, shift time by dt
//        c. Push to the new lane
//   burst(1, x) is identity — no extra copies.
//
// count and interval are frozen as varying signals so each event
// independently draws its own count and interval values.

inline MultiLaneLeaf<AudioEvent> burst(AudioSignal<int> countSig, AudioSignal<float> intervalSig){
    return MultiLaneLeaf<AudioEvent>(
        std::function<AudioMultiLane(AudioMultiLane)>(
            [countSig, intervalSig](AudioMultiLane in) -> AudioMultiLane {
                auto frozenCount    = countSig();
                auto frozenInterval = intervalSig();

                AudioMultiLane out;
                for (auto& lane : in) {
                    AudioLane newLane;
                    for (auto& e : lane) {
                        int n = frozenCount(e);
                        if (n < 1) n = 1;

                        newLane.push_back(e);

                        AudioEvent prev = e;
                        for (int i = 1; i < n; ++i) {
                            float dt = frozenInterval(e);
                            AudioEvent replica = prev;
                            replica.time += dt;
                            newLane.push_back(replica);
                            prev = replica;
                        }
                    }
                    out.push_back(std::move(newLane));
                }
                return out;
            }
        )
    );
}
inline MultiLaneLeaf<AudioEvent> burst(int count, float interval) {
    return burst(constant(count), constant(interval));
}
inline MultiLaneLeaf<AudioEvent> burst(AudioSignal<int> count, float interval) {
    return burst(count, constant(interval));
}
inline MultiLaneLeaf<AudioEvent> burst(int count, AudioSignal<float> interval) {
    return burst(constant(count), interval);
}


// ntole - repeat a Multilane n times uniformly spaced within a total duration

inline MultiLaneLeaf<AudioEvent> ntole(AudioSignal<int> nSig, AudioSignal<float> durationSig) {
    return MultiLaneLeaf<AudioEvent>(
        std::function<AudioMultiLane(AudioMultiLane)>(
            [nSig, durationSig](AudioMultiLane in) -> AudioMultiLane {
                auto frozenN    = nSig();
                auto frozenDuration = durationSig();

                AudioMultiLane out;
                for (auto& lane : in) {
                    AudioLane newLane;
                    for (auto& e : lane) {
                        int n = frozenN(e);
                        if (n < 1) n = 1;
                        float dt = frozenDuration(e) / n;

                        newLane.push_back(e);

                        AudioEvent prev = e;
                        for (int i = 1; i < n; ++i) {
                            AudioEvent replica = prev;
                            replica.time += dt;
                            newLane.push_back(replica);
                            prev = replica;
                        }
                    }
                    out.push_back(std::move(newLane));
                }
                return out;
            }
        )
    );
}

inline MultiLaneLeaf<AudioEvent> ntole(int n, float duration) {
    return ntole(constant(n), constant(duration));
}
inline MultiLaneLeaf<AudioEvent> ntole(AudioSignal<int> n, float duration) {
    return ntole(n, constant(duration));
}
inline MultiLaneLeaf<AudioEvent> ntole(int n, AudioSignal<float> duration) {
    return ntole(constant(n), duration);
}


}