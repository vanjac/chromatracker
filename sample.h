#pragma once
#include <common.h>

#include "units.h"
#include <array>
#include <shared_mutex>
#include <glm/glm.hpp>

namespace chromatracker {

struct Sample
{
    enum class InterpolationMode
    {
        Smooth = 0,
        Crunchy = 1,
    };
    enum class LoopMode
    {
        Once = 0,
        Forward = 1,
        PingPong = 2,
    };
    enum class NewNoteAction
    {
        Stop = 0,
        Fade = 1,
        Continue = 2,
    };

    mutable nevercopy<std::shared_mutex> mu;

    string name;
    glm::vec3 color {1, 1, 1};

    vector<vector<float>> channels;
    frames frameRate {48000};
    InterpolationMode interpolationMode { InterpolationMode::Smooth };

    LoopMode loopMode { LoopMode::Once };
    frames loopStart {0}, loopEnd {0}; // end is exclusive

    float volume {1.0};
    float tune {0}; // transpose + finetune, in semitones

    NewNoteAction newNoteAction { NewNoteAction::Stop };

    // special

    // 0 - 1, velocity units down per tick
    // UI knob for this value is exponential
    float fadeOut {1.0}; // TODO better default value
};

} // namespace
