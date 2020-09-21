#include "Units.h"
#include "InstSample.h"

namespace chromatracker {

InstSample::InstSample() :
        name(""),
        wave(nullptr),
        wave_channels(1),
        wave_frames(0),
        wave_frame_rate(48000),
        playback_mode(PlaybackMode::ONCE),
        loop_type(LoopType::FORWARD),
        loop_start(0), loop_end(0),
        volume(1.0f),
        panning(0.0f),
        base_key(MIDDLE_C),
        finetune(0.0f),
        key_start(MIN_KEY), key_end(MAX_KEY) { }

}
