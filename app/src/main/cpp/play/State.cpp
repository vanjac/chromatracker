#include "State.h"

SongState::SongState(int out_frame_rate) :
        out_frame_rate(out_frame_rate),
        global_time(0) {
    std::random_device rand_dev;
    this->random.seed(rand_dev());
}