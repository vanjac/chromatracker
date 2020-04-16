#include <math.h>
#include "playback.h"

float note_rate(int note) {
    // TODO slow?
    return exp2f((note - MIDDLE_C) / 12.0);
}

void init_channel_playback(ChannelPlayback * channel) {
    channel->instrument = NULL;
    channel->playback_rate = 0;
    channel->playback_pos = 0;
    channel->note_state = PLAY_OFF;
    channel->volume = 1.0;
    channel->control_command = NO_ID;
    channel->ctl_vel_up = 0;
    channel->ctl_vel_down = 0;
}

void init_track_playback(TrackPlayback * track) {
    track->channel = NULL;
    track->pattern = NULL;
    track->pattern_tick = 0;
    track->event_i = 0;
}

void init_song_playback(SongPlayback * playback, Song * song) {
    playback->song = song;

    playback->current_page = 0;
    playback->current_page_ticks = 0;

    playback->tick_len = 120<<16; // 125 bpm TODO
    playback->tick_len_error = 0;

    playback->num_channels = song->num_tracks;
    playback->channels = (ChannelPlayback *)malloc(playback->num_channels * sizeof(ChannelPlayback));
    for (int i = 0; i < playback->num_channels; i++)
        init_channel_playback(&playback->channels[i]);
    playback->num_tracks = song->num_tracks;
    playback->tracks = (TrackPlayback *)malloc(playback->num_tracks * sizeof(TrackPlayback));
    for (int i = 0; i < playback->num_tracks; i++) {
        init_track_playback(&playback->tracks[i]);
        playback->tracks[i].channel = &playback->channels[i];
    }

    set_playback_page(playback, 0);
}

void free_song_playback(SongPlayback * playback) {
    if (playback->channels) {
        free(playback->channels);
        playback->channels = NULL;
    }
    if (playback->tracks) {
        free(playback->tracks);
        playback->tracks = NULL;
    }
}

void set_playback_page(SongPlayback * playback, int page) {
    Song * song = playback->song;
    page %= song->num_pages;
    playback->current_page = page;
    playback->current_page_ticks = 0;
    for (int i = 0; i < playback->num_tracks; i++) {
        TrackPlayback * track = &playback->tracks[i];
        int pattern_num = song->tracks[i].pages[page];
        if (pattern_num == NO_PATTERN)
            track->pattern = NULL;
        else
            track->pattern = &song->tracks[i].patterns[pattern_num];
        track->pattern_tick = 0;
        track->event_i = 0;
    }
}