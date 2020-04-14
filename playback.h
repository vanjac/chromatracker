#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "chroma.h"
#include "pattern.h"
#include "instrument.h"

float note_rate(int note);

typedef struct {
    InstSample * instrument;
    Sint32 playback_rate; // fp 16.16 sample rate
    Sint64 playback_pos; // fp 32.16 sample num
    enum {PLAY_OFF, PLAY_ON, PLAY_RELEASE} note_state;
    float volume;

    Uint16 control_command;
    Uint8 ctl_vel_up, ctl_vel_down;
} ChannelPlayback;

typedef struct {
    ChannelPlayback * channel;
    Pattern * pattern;
    int pattern_tick;
    int event_i;
} TrackPlayback;

#endif