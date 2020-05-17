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

    float pitch_semis; // pitch, 1.0 = 1 semitone
    Sint32 playback_rate; // fp 16.16 frame rate, calculated from pitch
    Sint64 playback_pos; // fp 32.16 frame num

    float volume;

    float vel_slide, pitch_slide;
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

    int tick_len; // fp 16.16 frame length
    int tick_len_error; // fp 16.16 accumulated error in tick length

    ChannelPlayback * channels;
    int num_channels;
    TrackPlayback * tracks;
    int num_tracks;
} SongPlayback;

void init_song_playback(SongPlayback * playback, Song * song, int out_freq);
void free_song_playback(SongPlayback * playback);

// return tick length (num frames written to buffer)
int process_tick(SongPlayback * playback, StereoFrame * tick_buffer);
void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay);

#endif