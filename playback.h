#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "chroma.h"
#include "pattern.h"
#include "instrument.h"
#include "song.h"

typedef enum {PLAY_OFF, PLAY_ON, PLAY_RELEASE} NoteState;

typedef struct {
    InstSample * instrument;
    NoteState note_state;

    float pitch_octaves; // pitch, 1/12.0 = 1 semitone
    Sint32 playback_rate; // fp 16.16 sample rate, calculated from pitch_cents
    Sint64 playback_pos; // fp 32.16 sample num

    float volume;

    Uint16 control_command;
    float vel_slide, target_vel;
    float pitch_slide, target_pitch;
} ChannelPlayback;

void init_channel_playback(ChannelPlayback * channel);

typedef struct {
    ChannelPlayback * channel;
    Pattern * pattern;
    int pattern_tick;
    int event_i;

    Uint8 control_memory[MAX_CONTROL_INDEX];
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
void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay);

#endif