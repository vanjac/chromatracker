#include <math.h>
#include "playback.h"

#define SAMPLE_MASTER_VOLUME 0.5

static void set_playback_page(SongPlayback * playback, int page);
static void process_tick_track(TrackPlayback * track, SongPlayback * playback);
static void process_tick_channel(ChannelPlayback * c, SongPlayback * playback, StereoFrame * tick_buffer, int tick_buffer_len);
static void process_effect(char effect, Uint8 value, ChannelPlayback * channel);
static Sint32 calc_playback_rate(int out_freq, float c5_freq, float pitch_semis);


ChannelPlayback::ChannelPlayback() 
: instrument(NULL), note_state(PLAY_OFF),
pitch_semis(0.0), playback_rate(0), playback_pos(0),
volume(1.0),
vel_slide(0), pitch_slide(0) { }

TrackPlayback::TrackPlayback()
: channel(NULL),
pattern(NULL), pattern_tick(0), event_i(0) { }

SongPlayback::SongPlayback(int out_freq)
: song(NULL), out_freq(out_freq),
current_page(0), current_page_tick(0),
tick_len(120<<16), tick_len_error(0),  // 125 bpm TODO
channels(NULL), num_channels(0),
tracks(NULL), num_tracks(0) { }

SongPlayback::~SongPlayback() {
    delete [] channels;
    delete [] tracks;
}


void set_playback_song(SongPlayback * playback, Song * song) {
    playback->song = song;

    playback->num_channels = song->num_tracks;
    playback->channels = new ChannelPlayback[playback->num_channels];
    playback->num_tracks = song->num_tracks;
    playback->tracks = new TrackPlayback[playback->num_tracks];

    for (int i = 0; i < playback->num_tracks; i++)
        playback->tracks[i].channel = &playback->channels[i];

    set_playback_page(playback, 0);
}

void set_playback_page(SongPlayback * playback, int page) {
    Song * song = playback->song;
    page %= song->num_pages;
    playback->current_page = page;
    playback->current_page_tick = 0;
    for (int i = 0; i < playback->num_tracks; i++) {
        TrackPlayback * track = &playback->tracks[i];
        int pattern_num = song->tracks[i].pages[page];
        track->pattern = &song->tracks[i].patterns[pattern_num];
        track->pattern_tick = 0;
        track->event_i = 0;
    }
}


int process_tick(SongPlayback * playback, StereoFrame * tick_buffer) {
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
        process_tick_channel(&playback->channels[i], playback, tick_buffer, tick_buffer_len);

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


void process_tick_channel(ChannelPlayback * c, SongPlayback * playback, StereoFrame * tick_buffer, int tick_buffer_len) {
    if (c->note_state == PLAY_OFF)
        return;

    InstSample * inst = c->instrument;

    StereoFrame * write = tick_buffer;
    StereoFrame * tick_buffer_end = tick_buffer + tick_buffer_len;
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
            StereoFrame mix_frame = inst->wave[c->playback_pos >> 16];
            write->l += mix_frame.l * c->volume * SAMPLE_MASTER_VOLUME;
            write->r += mix_frame.r * c->volume * SAMPLE_MASTER_VOLUME;
            write++;
            c->playback_pos += c->playback_rate;
        }
        if (loop)
            c->playback_pos -= (Sint64)(inst->loop_end - inst->loop_start) << 16;
    }

    // these control commands go after tick

    c->volume += c->vel_slide;
    if (c->volume > 1.0)
        c->volume = 1.0;
    else if (c->volume < 0.0)
        c->volume = 0.0;
    
    if (c->pitch_slide != 0.0) {
        c->pitch_semis += c->pitch_slide;
        c->playback_rate = calc_playback_rate(playback->out_freq,
            c->instrument->c5_freq, c->pitch_semis);
    }
}


void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay) {
    // TODO use tick delay!!
    ChannelPlayback * channel = track->channel;

    char inst_special = event.instrument[0];
    if (inst_special == EVENT_NOTE_CUT) {
        channel->note_state = PLAY_OFF;
    } else {
        if (!instrument_is_special(event)) {
            // note on
            InstSample * inst = get_instrument(playback->song, event.instrument);
            if (inst) {
                channel->instrument = inst;
                channel->playback_pos = 0;
                channel->note_state = PLAY_ON;
                channel->pitch_semis = inst->default_pitch;
                channel->volume = inst->default_velocity / (float)MAX_VELOCITY;
            }
        } else if (inst_special == EVENT_REPLAY) {
            // use previous instrument, don't set defaults
            if (channel->instrument) {
                channel->playback_pos = 0;
                channel->note_state = PLAY_ON;
            }
        }

        // reset previous effects (TODO: special case for glide)
        channel->pitch_slide = 0.0;
        channel->vel_slide = 0.0;
        process_effect(event.p_effect, event.p_value, channel);
        process_effect(event.v_effect, event.v_value, channel);

        if (channel->instrument)
            channel->playback_rate = calc_playback_rate(playback->out_freq,
                channel->instrument->c5_freq, channel->pitch_semis);
    }
}


void process_effect(char effect, Uint8 value, ChannelPlayback * channel) {
    switch (effect) {
        case EFFECT_PITCH:
            // TODO check for glide
            channel->pitch_semis = value;
            break;
        case EFFECT_VELOCITY:
            channel->volume = value / (float)MAX_VELOCITY;
            break;
        case EFFECT_TUNE:
            channel->pitch_semis += (value - 0x40) / (float)0x40;
            break;
        case EFFECT_SAMPLE_OFFSET:
            if (channel->instrument) {
                if (value < channel->instrument->num_slices) {
                    channel->playback_pos = (Sint64)channel->instrument->slices[value] << 16;
                } else {
                    channel->playback_pos = ((Sint64)channel->instrument->wave_len * value / 256) << 16;
                }
            }
            break;
    }
}


Sint32 calc_playback_rate(int out_freq, float c5_freq, float pitch_semis) {
    float note_rate = exp2f((pitch_semis - MIDDLE_C) / 12.0);
    return (Sint32)roundf(note_rate * c5_freq / out_freq * 65536);
}