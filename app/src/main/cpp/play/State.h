#ifndef CHROMATRACKER_STATE_H
#define CHROMATRACKER_STATE_H

#include <random>

struct SongState {
    SongState(int out_frame_rate);

    std::default_random_engine random;
    int out_frame_rate;
    int global_time;
};

#endif //CHROMATRACKER_STATE_H
