#ifndef CHROMATRACKER_INSTSAMPLE_H
#define CHROMATRACKER_INSTSAMPLE_H

#include <memory>
#include <string>

namespace chromatracker {

enum class PlaybackMode {
    ONCE, LOOP, SUSTAIN_LOOP
};

enum class LoopType {
    FORWARD, PING_PONG
};

struct InstSample {
    InstSample();

    std::string name;

    std::unique_ptr<float[]> wave;
    int wave_channels;
    int wave_frames;
    int wave_frame_rate;

    PlaybackMode playback_mode;
    LoopType loop_type;
    int loop_start, loop_end;  // frame, end is exclusive

    float volume;
    float panning;  // -1.0 to 1.0
    int base_key;
    float finetune;  // from -1.0 to 1.0, in semitones
    int key_start, key_end;  // inclusive
};

}

#endif //CHROMATRACKER_INSTSAMPLE_H
