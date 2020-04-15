#include <SDL2/SDL.h>
#include <stdio.h>

#include "chroma.h"
#include "pattern.h"
#include "playback.h"
#include "instrument.h"
#include "song.h"
#include "load_mod.h"

#define OUT_FREQ 48000

#define SAMPLE_MASTER_VOLUME 0.5

Song song;

// 125 bpm
int tick_len = 120<<16; // fp 16.16 sample length
int tick_len_error = 0; // fp 16.16 accumulated error in tick length

// enough for 15 BPM at 48000Hz
#define MAX_TICK_BUFFER 1024
Sample tick_buffer[MAX_TICK_BUFFER];
int tick_buffer_len = 0;
int tick_buffer_pos = 0;

#define NUM_CHANNELS 8
ChannelPlayback channels[NUM_CHANNELS];

volatile Uint32 audio_callback_time;

#define NUM_KEYS 32
ChannelPlayback * keyboard_instruments[NUM_KEYS];

int note_keymap(SDL_Keycode key);
Sint32 calc_playback_rate(int out_freq, float c5_freq, float rate);
ChannelPlayback * find_empty_channel(void);
void callback(void * userdata, Uint8 * stream, int len);
void process_tick(void);
void process_tick_track(TrackPlayback * track);
void process_tick_channel(ChannelPlayback * c);
void process_event(Event event, ChannelPlayback * channel, int tick_delay);

#define NUM_TRACKS 4
TrackPlayback track_states[NUM_TRACKS] = {
    {&channels[0], NULL, 0, 0},
    {&channels[1], NULL, 0, 0},
    {&channels[2], NULL, 0, 0},
    {&channels[3], NULL, 0, 0}
};


int current_page = -1;
int current_page_ticks = 0;


int main(int argv, char ** argc) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window * window = SDL_CreateWindow("chromatracker",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        0);
    if (window == NULL) {
        printf("Couldn't create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec spec;
    spec.freq = OUT_FREQ;
    spec.format = AUDIO_F32;
    spec.channels = 2;
    spec.samples = 128;
    spec.callback = &callback;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!device) {
        printf("Can't open audio %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    load_mod("mod.resonance", &song);

    for (int i = 0; i < NUM_CHANNELS; i++)
        channels[i].note_state = PLAY_OFF;
    audio_callback_time = SDL_GetTicks();
    SDL_PauseAudioDevice(device, 0); // audio devices start paused

    for (int i = 0; i < NUM_KEYS; i++)
        keyboard_instruments[i] = 0;

    int running = 1;
    int inst_select = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            int time_offset = SDL_GetTicks() - audio_callback_time;
            int tick_delay = ((OUT_FREQ * time_offset / 1000) << 16) / tick_len;

            if (event.type == SDL_QUIT)
                running = 0;
            if (event.type == SDL_KEYDOWN) {
                int key = note_keymap(event.key.keysym.sym);
                if (key >= 0 && !keyboard_instruments[key]) {
                    ChannelPlayback * channel = find_empty_channel();
                    // tick time: tick_len / spec.freq
                    Event key_event = {0, inst_select, key + 5*12, 100, 0};
                    process_event(key_event, channel, tick_delay);
                    keyboard_instruments[key] = channel;
                } else {
                    if (event.key.keysym.sym == SDLK_EQUALS) {
                        inst_select++;
                        printf("instrument %d\n", inst_select);
                    } else if (event.key.keysym.sym == SDLK_MINUS) {
                        inst_select--;
                        printf("instrument %d\n", inst_select);
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                int key = note_keymap(event.key.keysym.sym);
                if (key >= 0 && keyboard_instruments[key]) {
                    ChannelPlayback * channel = keyboard_instruments[key];
                    Event key_event = {0, NOTE_CUT, 0, 0, 0};
                    process_event(key_event, channel, tick_delay);
                    keyboard_instruments[key] = 0;
                }
            }
        }
    }
    printf("no\n");
    // stop callbacks
    SDL_PauseAudioDevice(device, 1);

    free_song(&song);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    return 0;
}


int note_keymap(SDL_Keycode key) {
    switch(key) {
        case SDLK_z:
            return 0;
        case SDLK_s:
            return 1;
        case SDLK_x:
            return 2;
        case SDLK_d:
            return 3;
        case SDLK_c:
            return 4;
        case SDLK_v:
            return 5;
        case SDLK_g:
            return 6;
        case SDLK_b:
            return 7;
        case SDLK_h:
            return 8;
        case SDLK_n:
            return 9;
        case SDLK_j:
            return 10;
        case SDLK_m:
            return 11;
        case SDLK_COMMA:
        case SDLK_q:
            return 12;
        case SDLK_l:
        case SDLK_2:
            return 13;
        case SDLK_PERIOD:
        case SDLK_w:
            return 14;
        case SDLK_SEMICOLON:
        case SDLK_3:
            return 15;
        case SDLK_SLASH:
        case SDLK_e:
            return 16;
        case SDLK_r:
            return 17;
        case SDLK_5:
            return 18;
        case SDLK_t:
            return 19;
        case SDLK_6:
            return 20;
        case SDLK_y:
            return 21;
        case SDLK_7:
            return 22;
        case SDLK_u:
            return 23;
        case SDLK_i:
            return 24;
        case SDLK_9:
            return 25;
        case SDLK_o:
            return 26;
        case SDLK_0:
            return 27;
        case SDLK_p:
            return 28;
        default:
            return -1;
    }
}


Sint32 calc_playback_rate(int out_freq, float c5_freq, float rate) {
    return (Sint32)(rate * c5_freq / out_freq * 65536);
}


ChannelPlayback * find_empty_channel(void) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ChannelPlayback * c = &channels[i];
        if (c->note_state == PLAY_OFF)
            return c;
    }
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ChannelPlayback * c = &channels[i];
        if (c->note_state == PLAY_RELEASE)
            return c;
    }
    return &channels[0]; // TODO
}


void callback(void * userdata, Uint8 * stream, int len) {
    audio_callback_time = SDL_GetTicks();

    Sample * sample_stream = (Sample *)stream;
    int sample_stream_len = len / sizeof(Sample);
    Sample * sample_stream_end = sample_stream + sample_stream_len;

    while (1) {
        if (tick_buffer_pos < tick_buffer_len) {
            int write_len = tick_buffer_len - tick_buffer_pos;
            if (write_len > (sample_stream_end - sample_stream)) {
                write_len = sample_stream_end - sample_stream;
            }
            for (int i = 0; i < write_len; i++)
                *(sample_stream++) = tick_buffer[tick_buffer_pos++];
        }

        if (sample_stream >= sample_stream_end)
            break;

        process_tick();
        tick_buffer_pos = 0;
    }
}


void process_tick(void) {
    // process page
    if (current_page == -1
            || current_page_ticks >= song.page_lengths[current_page]) {
        current_page++;
        if (current_page == song.num_pages)
            current_page = 0;
        printf("Page %d\n", current_page);
        current_page_ticks = 0;
        for (int i = 0; i < NUM_TRACKS; i++) {
            int pattern_num = song.tracks[i].pages[current_page];
            if (pattern_num == NO_PATTERN)
                track_states[i].pattern = NULL;
            else
                track_states[i].pattern = &(song.tracks[i].patterns[pattern_num]);
        }
    }
    current_page_ticks++;

    // process events
    for (int i = 0; i < NUM_TRACKS; i++)
        process_tick_track(&track_states[i]);

    // process audio

    tick_buffer_len = tick_len >> 16;
    tick_len_error += tick_len & 0xFFFF;
    if (tick_len_error >= (1<<16)) {
        tick_len_error -= 1<<16;
        tick_buffer_len += 1;
    }

    for (int i = 0; i < tick_buffer_len; i++) {
        tick_buffer[i].l = 0;
        tick_buffer[i].r = 0;
    }

    for (int i = 0; i < NUM_CHANNELS; i++)
        process_tick_channel(&channels[i]);
}


void process_tick_track(TrackPlayback * track) {
    Pattern * pattern = track->pattern;
    if (!pattern)
        return;
    while (1) {
        if (track->event_i >= pattern->num_events)
            break;
        Event next_event = pattern->events[track->event_i];
        if (next_event.time == track->pattern_tick) {
            track->event_i++;
            // play event
            ChannelPlayback * channel = track->channel;
            process_event(next_event, channel, 0);
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


void process_tick_channel(ChannelPlayback * c) {
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
            c->volume += VELOCITY_SLIDE_SCALE * c->ctl_vel_up;
            if (c->volume > 1.0)
                c->volume = 1.0;
            break;
        case CTL_VEL_DOWN:
            c->volume -= VELOCITY_SLIDE_SCALE * c->ctl_vel_down;
            if (c->volume < 0.0)
                c->volume = 0.0;
            break;
    }
}


void process_event(Event event, ChannelPlayback * channel, int tick_delay) {
    // TODO use tick delay!!
    float rate = note_rate(event.pitch);

    int inst_col = event.inst_control & INST_MASK;
    if (inst_col == NOTE_CUT) {
        channel->note_state = PLAY_OFF;
    } else {
        if (inst_col != NO_ID) {
            // note on
            InstSample * inst = get_instrument(&song, inst_col);
            if (inst) {
                channel->instrument = inst;
                channel->playback_pos = 0;
                channel->note_state = PLAY_ON;
            }
        }
        
        if (event.pitch != NO_PITCH && channel->instrument)
            channel->playback_rate = calc_playback_rate(OUT_FREQ, channel->instrument->c5_freq, rate);
        if (event.velocity != NO_VELOCITY)
            channel->volume = event.velocity / 100.0;

        channel->control_command = event.inst_control & CONTROL_MASK;
        switch (channel->control_command & ~CTL_MODULATION) {
            case CTL_VEL_UP:
                channel->ctl_vel_up = event.param;
                break;
            case CTL_VEL_DOWN:
                channel->ctl_vel_down = event.param;
                break;
        }
    }
}
