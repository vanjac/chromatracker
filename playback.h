#ifndef PLAYBACK_H
#define PLAYBACK_H

#include "chroma.h"
#include "pattern.h"
#include "instrument.h"
#include "song.h"

enum NoteState {PLAY_OFF, PLAY_ON, PLAY_RELEASE};

struct ChannelPlayback {
    InstSample * instrument;
    NoteState note_state;

    float pitch_semis; // pitch, 1.0 = 1 semitone
    Sint32 playback_rate; // fp 16.16 frame rate, calculated from pitch
    Sint64 playback_pos; // fp 32.16 frame num

    float volume;
    float pan; // 0.0 left, 0.5 center, 1.0 right

    float vel_slide, pitch_slide;
    float glide_pitch; // negative if no glide
    int vibrato_i, vibrato_rate;
    float vibrato_depth;

    ChannelPlayback();
};

struct TrackPlayback {
    ChannelPlayback * channel;
    Pattern * pattern;
    int pattern_tick;
    int event_i;  // next event

    TrackPlayback();
};

struct SongPlayback {
    Song * song;

    bool is_playing;

    int current_page;
    int current_page_tick;

    int out_freq;

    int tick_len; // fp 16.16 frame length
    int tick_len_error; // fp 16.16 accumulated error in tick length

    ChannelPlayback * channels;
    int num_channels;
    TrackPlayback * tracks;
    int num_tracks;

    SongPlayback(int out_freq);
    ~SongPlayback();
};

void set_playback_song(SongPlayback * playback, Song * song);
void set_playback_page(SongPlayback * playback, int page);
void process_song_tick(SongPlayback * playback);
// return tick length (num frames written to buffer)
int process_audio_tick(SongPlayback * playback, StereoFrame * tick_buffer);
void all_tracks_off(SongPlayback * playback);
void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay);

#endif