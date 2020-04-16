#include <SDL.h>
#include <GL/gl3w.h> 
#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include "chroma.h"
#include "pattern.h"
#include "playback.h"
#include "instrument.h"
#include "song.h"
#include "load_mod.h"
#include "guimain.h"

#define OUT_FREQ 48000

#define SAMPLE_MASTER_VOLUME 0.5

Song song;
SongPlayback playback;

// enough for 15 BPM at 48000Hz
#define MAX_TICK_BUFFER 1024
Sample tick_buffer[MAX_TICK_BUFFER];
int tick_buffer_len = 0;
int tick_buffer_pos = 0;

volatile Uint32 audio_callback_time;

#define NUM_KEYS 32
ChannelPlayback * keyboard_instruments[NUM_KEYS];

Sint8 note_keymap(SDL_Keycode key);
Sint32 calc_playback_rate(int out_freq, float c5_freq, float rate);
ChannelPlayback * find_empty_channel(SongPlayback * playback);
void callback(void * userdata, Uint8 * stream, int len);
void process_tick(SongPlayback * playback);
void process_tick_track(TrackPlayback * track);
void process_tick_channel(ChannelPlayback * c);
void process_event(Event event, ChannelPlayback * channel, int tick_delay);


int main(int argv, char ** argc) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window * window = SDL_CreateWindow("chromatracker",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL);
    if (!window) {
        printf("Couldn't create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("Couldn't create OpenGL context: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1); // Enable vsync

    if (gl3wInit()) {
        printf("Couldn't load OpenGL!\n");
        SDL_Quit();
        return 1;
    }

    // https://github.com/ocornut/imgui/blob/master/examples/example_sdl_opengl3/main.cpp
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");


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

    load_mod("space_debris.mod", &song);

    init_song_playback(&playback, &song);

    audio_callback_time = SDL_GetTicks();
    SDL_PauseAudioDevice(device, 0); // audio devices start paused

    for (int i = 0; i < NUM_KEYS; i++)
        keyboard_instruments[i] = 0;

    int running = 1;
    Uint16 inst_select = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            int time_offset = SDL_GetTicks() - audio_callback_time;
            int tick_delay = ((OUT_FREQ * time_offset / 1000) << 16) / playback.tick_len;

            if (event.type == SDL_QUIT)
                running = 0;
            if (event.type == SDL_KEYDOWN) {
                Sint8 key = note_keymap(event.key.keysym.sym);
                if (key >= 0 && !keyboard_instruments[key]) {
                    ChannelPlayback * channel = find_empty_channel(&playback);
                    // tick time: tick_len / spec.freq
                    Event key_event = {0, inst_select, (Sint8)(key + 5*12), 100, 0};
                    process_event(key_event, channel, tick_delay);
                    keyboard_instruments[key] = channel;
                } else {
                    if (event.key.keysym.sym == SDLK_EQUALS) {
                        inst_select++;
                        printf("instrument %hu\n", inst_select);
                    } else if (event.key.keysym.sym == SDLK_MINUS) {
                        inst_select--;
                        printf("instrument %hu\n", inst_select);
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        gui();
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    printf("no\n");
    // stop callbacks
    SDL_PauseAudioDevice(device, 1);

    free_song_playback(&playback);
    free_song(&song);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(device);
    SDL_Quit();
    return 0;
}


Sint8 note_keymap(SDL_Keycode key) {
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


ChannelPlayback * find_empty_channel(SongPlayback * playback) {
    for (int i = 0; i < playback->num_channels; i++) {
        ChannelPlayback * c = &playback->channels[i];
        if (c->note_state == PLAY_OFF)
            return c;
    }
    for (int i = 0; i < playback->num_channels; i++) {
        ChannelPlayback * c = &playback->channels[i];
        if (c->note_state == PLAY_RELEASE)
            return c;
    }
    return &playback->channels[0]; // TODO
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

        process_tick(&playback);
        tick_buffer_pos = 0;
    }
}


void process_tick(SongPlayback * playback) {
    // process page
    Song * song = playback->song;
    if (playback->current_page_ticks >= song->page_lengths[playback->current_page]) {
        set_playback_page(playback, playback->current_page + 1);
    }
    playback->current_page_ticks++;

    // process events
    for (int i = 0; i < playback->num_tracks; i++)
        process_tick_track(&playback->tracks[i]);

    // process audio

    tick_buffer_len = playback->tick_len >> 16;
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
        process_tick_channel(&playback->channels[i]);
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
        
        if (event.pitch != NO_PITCH && channel->instrument) {
            float rate = note_rate(event.pitch);
            channel->playback_rate = calc_playback_rate(OUT_FREQ, channel->instrument->c5_freq, rate);
        }
        if (event.velocity != NO_VELOCITY)
            channel->volume = event.velocity / 100.0;

        channel->control_command = event.inst_control & CONTROL_MASK;
        Uint16 numeric = event.param & PARAM_NUM_MASK;
        switch (channel->control_command) {
            case CTL_VEL_UP:
                if (event.param & PARAM_IS_NUM)
                    channel->ctl_vel_up = numeric;
                break;
            case CTL_VEL_DOWN:
                if (event.param & PARAM_IS_NUM)
                    channel->ctl_vel_down = numeric;
                break;
            case CTL_SLICE:
                if ((event.param & PARAM_IS_NUM) && channel->instrument
                        && numeric < channel->instrument->num_slices)
                    channel->playback_pos = (Sint64)channel->instrument->slices[numeric] << 16;
                break;
        }
    }
}
