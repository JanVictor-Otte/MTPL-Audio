#pragma once
#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/core/AudioSignal.hpp"
#include <cmath>

namespace mtpl {

// gain — per-event: multiply gain by factor
inline EventLeaf<AudioEvent, float> gain(AudioSignal<float> factor) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float f) -> AudioEvent {
            e.payload.gain *= f;
            return e;
        },
        factor
    );
}
inline EventLeaf<AudioEvent, float> gain(float v) { return gain(constant(v)); }

// gainDb — per-event: multiply gain by 10^(db/20)
inline EventLeaf<AudioEvent, float> gainDb(AudioSignal<float> db) {
    return EventLeaf<AudioEvent, float>(
        [](AudioEvent e, float d) -> AudioEvent {
            e.payload.gain *= std::pow(10.f, d / 20.f);
            return e;
        },
        db
    );
}
inline EventLeaf<AudioEvent, float> gainDb(float v) { return gainDb(constant(v)); }

// envelope — lane-level: linear gain ramp across events in each lane
inline MultiLaneLeaf<AudioEvent> envelope(float gainStart, float gainEnd) {
    return MultiLaneLeaf<AudioEvent>(
        std::function<AudioMultiLane(AudioMultiLane)>(
            [gainStart, gainEnd](AudioMultiLane in) -> AudioMultiLane {
                AudioMultiLane out;
                for (auto& lane : in) {
                    AudioLane outLane = lane;
                    int n = (int)outLane.size();
                    if (n > 1)
                        for (int i = 0; i < n; ++i) {
                            float t = (float)i / (float)(n - 1);
                            outLane[i].payload.gain *= gainStart + t * (gainEnd - gainStart);
                        }
                    else if (n == 1)
                        outLane[0].payload.gain *= gainStart;
                    out.push_back(outLane);
                }
                return out;
            }
        )
    );
}
inline MultiLaneLeaf<AudioEvent> fadeIn()  { return envelope(0.f, 1.f); }
inline MultiLaneLeaf<AudioEvent> fadeOut() { return envelope(1.f, 0.f); }
}