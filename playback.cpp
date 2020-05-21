#include <math.h>
#include "playback.h"
#include "playback_lut.h"

#define SAMPLE_MASTER_VOLUME 0.5

static void set_playback_page(SongPlayback * playback, int page);
static void process_tick_track(TrackPlayback * track, SongPlayback * playback);
static void process_tick_channel(ChannelPlayback * c, SongPlayback * playback, StereoFrame * tick_buffer, int tick_buffer_len);
static void process_effect(char effect, Uint8 value, ChannelPlayback * channel, bool glide);
static int calc_tick_len(int tempo, int out_freq);
static Sint32 calc_playback_rate(int out_freq, float c5_freq, float pitch_semis);
static float calc_slide_rate(Uint8 effect_value, int bias);


ChannelPlayback::ChannelPlayback() 
: instrument(NULL), note_state(PLAY_OFF),
pitch_semis(0.0), playback_rate(0), playback_pos(0),
volume(1.0), pan(0.0),
vel_slide(0), pitch_slide(0), glide_pitch(-1.0),
vibrato_i(0), vibrato_rate(0), vibrato_depth(0.0) { }

TrackPlayback::TrackPlayback()
: channel(NULL),
pattern(NULL), pattern_tick(0), event_i(0) { }

SongPlayback::SongPlayback(int out_freq)
: song(NULL), out_freq(out_freq),
current_page(0), current_page_tick(0),
tick_len(calc_tick_len(DEFAULT_TEMPO, out_freq)), tick_len_error(0),
channels(NULL), num_channels(0),
tracks(NULL), num_tracks(0) { }

SongPlayback::~SongPlayback() {
    delete [] channels;
    delete [] tracks;
}


void set_playback_song(SongPlayback * playback, Song * song) {
    playback->song = song;

    playback->num_channels = song->tracks.size();
    playback->channels = new ChannelPlayback[playback->num_channels];
    playback->num_tracks = song->tracks.size();
    playback->tracks = new TrackPlayback[playback->num_tracks];

    for (int i = 0; i < playback->num_tracks; i++)
        playback->tracks[i].channel = &playback->channels[i];
    // TODO default channel panning
    for (int i = 0; i < playback->num_channels; i++) {
        if (i % 4 == 0 || i % 4 == 3)
            playback->channels[i].pan = 0.25;
        else
            playback->channels[i].pan = 0.75;
    }

    set_playback_page(playback, 0);
}

void set_playback_page(SongPlayback * playback, int page) {
    Song * song = playback->song;
    if (song->num_pages == 0)
        return;
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
        if (track->event_i >= pattern->events.size())
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

        float left_vol = c->volume * SAMPLE_MASTER_VOLUME;
        float right_vol = c->volume * SAMPLE_MASTER_VOLUME;
        if (c->pan >= 1)
            c->pan = 1;
        if (c->pan <= 0)
            c->pan = 0;
        // when centered both channels will be 0.5 amplitude
        // TODO how does panning work?
        left_vol *= 1 - c->pan;
        right_vol *= c->pan;
        while (c->playback_pos < max_pos) {
            StereoFrame mix_frame = inst->wave[c->playback_pos >> 16];
            write->l += mix_frame.l * left_vol;
            write->r += mix_frame.r * right_vol;
            write++;
            c->playback_pos += c->playback_rate;
        }
        if (loop)
            c->playback_pos -= (Sint64)(inst->loop_end - inst->loop_start) << 16;
    }

    // these control commands go after tick
    bool recalc_rate = false;

    c->volume += c->vel_slide;
    if (c->volume > 1.0)
        c->volume = 1.0;
    else if (c->volume < 0.0)
        c->volume = 0.0;
    
    if (c->pitch_slide != 0.0) {
        c->pitch_semis += c->pitch_slide;
        if (c->glide_pitch >= 0.0) {
            // stop after glide
            if (c->pitch_slide >= 0.0) {
                if (c->pitch_semis > c->glide_pitch)
                    c->pitch_semis = c->glide_pitch;
            } else {
                if (c->pitch_semis < c->glide_pitch)
                    c->pitch_semis = c->glide_pitch;
            }
        }
        recalc_rate = true;
    }

    if (c->vibrato_rate > 0 && c->vibrato_depth > 0) {
        c->vibrato_i += c->vibrato_rate;
        c->vibrato_i %= MODULATION_SINE_POINTS;
        recalc_rate = false; // special variation
        float offset = c->vibrato_depth * MODULATION_SINE[c->vibrato_i];
        c->playback_rate = calc_playback_rate(playback->out_freq,
            c->instrument->c5_freq, c->pitch_semis + offset);
    }

    if (recalc_rate)
        c->playback_rate = calc_playback_rate(playback->out_freq,
            c->instrument->c5_freq, c->pitch_semis);
}


void process_event(Event event, SongPlayback * playback, TrackPlayback * track, int tick_delay) {
    // TODO use tick delay!!
    ChannelPlayback * channel = track->channel;

    char inst_special = event.instrument[0];
    if (inst_special == EVENT_PLAYBACK) {
        switch (event.p_effect) {
            case EFFECT_TEMPO:
                if (event.v_effect == EFFECT_VELOCITY)
                    playback->tick_len = calc_tick_len(event.v_value, playback->out_freq);
                else if (event.v_effect >= '0' && event.v_effect <= '9')
                    // upper digit
                    playback->tick_len = calc_tick_len(
                        ((event.v_effect - '0') << 8) + event.v_value, playback->out_freq);
                break;
        }
    } else if (inst_special == EVENT_NOTE_CUT) {
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

        // reset previous effects
        channel->pitch_slide = 0.0;
        channel->vel_slide = 0.0;
        channel->vibrato_depth = 0;
        channel->vibrato_rate = 0;
        process_effect(event.p_effect, event.p_value, channel, event.v_effect == EFFECT_GLIDE);
        process_effect(event.v_effect, event.v_value, channel, false);
        if (channel->vibrato_i != 0 && channel->vibrato_depth == 0.0)
            channel->vibrato_i = 0; // retrigger

        if (channel->instrument)
            channel->playback_rate = calc_playback_rate(playback->out_freq,
                channel->instrument->c5_freq, channel->pitch_semis);
    }
}


void process_effect(char effect, Uint8 value, ChannelPlayback * channel, bool glide) {
    switch (effect) {
        case EFFECT_PITCH:
            if (glide)
                channel->glide_pitch = value;
            else
                channel->pitch_semis = value;
            break;
        case EFFECT_VELOCITY:
            channel->volume = value / (float)MAX_VELOCITY;
            break;
        case EFFECT_PITCH_SLIDE_UP:
            channel->pitch_slide = calc_slide_rate(value, PITCH_SLIDE_BIAS);
            channel->glide_pitch = -1.0;
            break;
        case EFFECT_PITCH_SLIDE_DOWN:
            channel->pitch_slide = -calc_slide_rate(value, PITCH_SLIDE_BIAS);
            channel->glide_pitch = -1.0;
            break;
        case EFFECT_GLIDE:
            channel->pitch_slide = calc_slide_rate(value, PITCH_SLIDE_BIAS);
            if (channel->glide_pitch < channel->pitch_semis)
                channel->pitch_slide *= -1;
            break;
        case EFFECT_VIBRATO:
            channel->vibrato_depth = (value & 0xF) / 8.0;
            channel->vibrato_rate = value >> 4;
            break;
        case EFFECT_TUNE:
            channel->pitch_semis += (value - 0x40) / (float)0x40;
            break;
        case EFFECT_VEL_SLIDE_UP:
            // 2^7 = 128 = MAX_VELOCITY
            channel->vel_slide = calc_slide_rate(value, VELOCITY_SLIDE_BIAS + 7);
            break;
        case EFFECT_VEL_SLIDE_DOWN:
            channel->vel_slide = -calc_slide_rate(value, VELOCITY_SLIDE_BIAS + 7);
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


static int calc_tick_len(int tempo, int out_freq) {
    return (Sint64)(1<<16) * out_freq * 60 / tempo / TICKS_PER_QUARTER;
}

Sint32 calc_playback_rate(int out_freq, float c5_freq, float pitch_semis) {
    float note_rate = exp2f((pitch_semis - MIDDLE_C) / 12.0);
    return (Sint32)roundf(note_rate * c5_freq / out_freq * 65536);
}

float calc_slide_rate(Uint8 effect_value, int bias) {
    int exp = effect_value >> 4;
    int mant = effect_value & 0xF;
    exp += 127 - bias;
    Uint32 float_bits = (exp << 23) | (mant << 19);
    void * bit_ptr = &float_bits;
    float slide = *((float *)bit_ptr);
    slide /= TICKS_PER_QUARTER;
    return slide;
}