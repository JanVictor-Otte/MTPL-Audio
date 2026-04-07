#pragma once

#include "mtpl/mtpl.hpp"

#include "mtpl_audio/core/AudioPayload.hpp"
#include "mtpl_audio/core/AudioSignal.hpp"
#include "mtpl_audio/core/AudioSource.hpp"

#include "mtpl_audio/audio/Generator.hpp"
#include "mtpl_audio/audio/Dynamics.hpp"
#include "mtpl_audio/audio/Pitch.hpp"
#include "mtpl_audio/audio/Rhythm.hpp"
#include "mtpl_audio/audio/SoundBuffer.hpp"
#include "mtpl_audio/audio/Mixer.hpp"
#include "mtpl_audio/audio/Scheduler.hpp"

#if defined(__APPLE__)
#include "mtpl_audio/platform/AudioToolbox.hpp"
#endif

#if defined(__linux__)
#include "mtpl_audio/platform/Alsa.hpp"
#endif

#if defined(_WIN32)
#include "mtpl_audio/platform/Wasapi.hpp"
#endif

#if defined(__ANDROID__)
#include "mtpl_audio/platform/Oboe.hpp"
#endif