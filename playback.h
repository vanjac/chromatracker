#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "chroma.h"
#include "pattern.h"
#include "instrument.h"
#include "song.h"

typedef enum {PLAY_OFF, PLAY_ON, PLAY_RELEASE} NoteState;

typedef struct {
    InstSample * instrument;
    Sint32 playback_rate; // fp 16.16 sample rate
    Sint64 playback_pos; // fp 32.16 sample num
    NoteState note_state;
    float volume;

    Uint16 control_command;
    Uint8 ctl_vel_up, ctl_vel_down;
} ChannelPlayback;

void init_channel_playback(ChannelPlayback * channel);

typedef struct {
    ChannelPlayback * channel;
    Pattern * pattern;
    int pattern_tick;
    int event_i;
} TrackPlayback;

void init_track_playback(TrackPlayback * track);

typedef struct {
    Song * song;

    int current_page;
    int current_page_tick;

    int out_freq;

    int tick_len; // fp 16.16 sample length
    int tick_len_error; // fp 16.16 accumulated error in tick length

    ChannelPlayback * channels;
    int num_channels;
    TrackPlayback * tracks;
    int num_tracks;
} SongPlayback;

void init_song_playback(SongPlayback * playback, Song * song, int out_freq);
void free_song_playback(SongPlayback * playback);

// return tick length (num samples written to buffer)
int process_tick(SongPlayback * playback, Sample * tick_buffer);
void process_event(Event event, SongPlayback * playback, ChannelPlayback * channel, int tick_delay);

#endif