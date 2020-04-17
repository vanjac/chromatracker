#include <math.h>
#include "playback.h"

#define SAMPLE_MASTER_VOLUME 0.5

// num percentage points per 24 ticks
#define VELOCITY_SLIDE_SCALE (1.0 / 100.0 / 24.0)

static float note_rate(int note);
static void set_playback_page(SongPlayback * playback, int page);
static void process_tick_track(TrackPlayback * track, SongPlayback * playback);
static void process_tick_channel(ChannelPlayback * c, Sample * tick_buffer, int tick_buffer_len);
static Sint32 calc_playback_rate(int out_freq, float c5_freq, float rate);

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
    channel->ctl_vel_slide = 0.0;
}

void init_track_playback(TrackPlayback * track) {
    track->channel = NULL;
    track->pattern = NULL;
    track->pattern_tick = 0;
    track->event_i = 0;

    for (int i = 0; i < MAX_CONTROL_INDEX; i++)
        track->control_memory[i] = 0;
    track->control_memory[CONTROL_INDEX(CTL_TUNE)] = 50;
}

void init_song_playback(SongPlayback * playback, Song * song, int out_freq) {
    playback->song = song;

    playback->current_page = 0;
    playback->current_page_tick = 0;

    playback->tick_len = 120<<16; // 125 bpm TODO
    playback->tick_len_error = 0;

    playback->out_freq = out_freq;

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
    playback->current_page_tick = 0;
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



int process_tick(SongPlayback * playback, Sample * tick_buffer) {
    // process page
    Song * song = playback->song;
    if (playback->current_page_tick >= song->page_lengths[playback->current_page]) {
        set_playback_page(playback, playback->current_page + 1);
    }
    playback->current_page_tick++;

    // process events
    for (int i = 0; i < playback->num_tracks; i++)
        process_tick_track(&playback->tracks[i], playback);

    // process audio

    int tick_buffer_len = playback->tick_len >> 16;
    playback->tick_len_error += playback->tick_len & 0xFFFF;
    if (playback->tick_len_error >= (1<<16)) {
        playback->tick_len_error -= 1<<16;
        tick_buffer_len += 1;
    }

    for (int i = 0; i < tick_buffer_len; i++) {
        tick_buffer[i].l = 0;
        tick_buffer[i].r = 0;
    }

    for (int i = 0; i < playback->num_channels; i++)
        process_tick_channel(&playback->channels[i], tick_buffer, tick_buffer_len);

    return tick_buffer_len;
}


void process_tick_track(TrackPlayback * track, SongPlayback * playback) {
    Pattern * pattern = track->pattern;
    if (!pattern)
        return;
    while (1) {
        if (track->event_i >= pattern->num_events)
            break;
        Event next_event = pattern->events[track->event_i];
        if (next_event.time == track->pattern_tick) {
            track->event_i++;
            process_event(next_event, playback, track, 0);
        } else {
            break;
        }
    }

    track->pattern_tick++;
    // loop
    if (track->pattern_tick >= track->pattern->length) {
        track->pattern_tick = 0;
        track->event_i = 0;
    }
}


void process_tick_channel(ChannelPlayback * c, Sample * tick_buffer, int tick_buffer_len) {
    if (c->note_state == PLAY_OFF)
        return;

    InstSample * inst = c->instrument;

    Sample * write = tick_buffer;
    Sample * tick_buffer_end = tick_buffer + tick_buffer_len;
    while (c->note_state != PLAY_OFF && write < tick_buffer_end) {
        int loop = 0;
        Sint64 max_pos = c->playback_pos + (tick_buffer_end - write) * c->playback_rate;
        if (inst->playback_mode == SMP_LOOP) {
            if (max_pos > (Sint64)inst->loop_end << 16) {
                max_pos = (Sint64)inst->loop_end << 16;
                loop = 1;
            }
        } else {
            if (max_pos > (Sint64)inst->wave_len << 16) {
                max_pos = (Sint64)inst->wave_len << 16;
                c->note_state = PLAY_OFF;
            } 
        }

        while (c->playback_pos < max_pos) {
            Sample mix_sample = inst->wave[c->playback_pos >> 16];
            write->l += mix_sample.l * c->volume * SAMPLE_MASTER_VOLUME;
            write->r += mix_sample.r * c->volume * SAMPLE_MASTER_VOLUME;
            write++;
            c->playback_pos += c->playback_rate;
        }
        if (loop)
            c->playback_pos -= (Sint64)(inst->loop_end - inst->loop_start) << 16;
    }

    // these control commands go after tick (some could go before)

    switch (c->control_command) {
        case CTL_VEL_UP:
        case CTL_VEL_DOWN:
            c->volume += c->ctl_vel_slide;
            if (c->volume > 1.0)
                c->volume = 1.0;
            else if (c->volume < 0.0)
                c->volume = 0.0;
            break;
    }
}


void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay) {
    // TODO use tick delay!!
    ChannelPlayback * channel = track->channel;

    int inst_col = event.inst_control & INST_MASK;
    if (inst_col == NOTE_CUT) {
        channel->note_state = PLAY_OFF;
    } else {
        if (inst_col != NO_ID) {
            // note on
            InstSample * inst = get_instrument(playback->song, inst_col);
            if (inst) {
                channel->instrument = inst;
                channel->playback_pos = 0;
                channel->note_state = PLAY_ON;
            }
        }
        
        if (event.pitch != NO_PITCH && channel->instrument) {
            float rate = note_rate(event.pitch);
            channel->playback_rate = calc_playback_rate(playback->out_freq, channel->instrument->c5_freq, rate);
        }
        if (event.velocity != NO_VELOCITY)
            channel->volume = event.velocity / 100.0;

        Uint16 command = event.inst_control & CONTROL_MASK;
        channel->control_command = command;
        Uint16 numeric;
        if (event.param & PARAM_IS_NUM) {
            // store in memory
            numeric = event.param & PARAM_NUM_MASK;
            track->control_memory[CONTROL_INDEX(command)] = numeric;
        } else {
            // recall from memory
            numeric = track->control_memory[CONTROL_INDEX(command)];
        }

        switch (channel->control_command) {
            case CTL_VEL_UP:
                channel->ctl_vel_slide = numeric * VELOCITY_SLIDE_SCALE;
                break;
            case CTL_VEL_DOWN:
                channel->ctl_vel_slide = -numeric * VELOCITY_SLIDE_SCALE;
                break;
            case CTL_SLICE:
                if (channel->instrument && numeric < channel->instrument->num_slices)
                    channel->playback_pos = (Sint64)channel->instrument->slices[numeric] << 16;
                break;
        }
    }
}


Sint32 calc_playback_rate(int out_freq, float c5_freq, float rate) {
    return (Sint32)roundf(rate * c5_freq / out_freq * 65536);
}