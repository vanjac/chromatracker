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

Song song;
SongPlayback playback(OUT_FREQ);

// enough for 15 BPM at 48000Hz
#define MAX_TICK_BUFFER 1024
StereoFrame tick_buffer[MAX_TICK_BUFFER];
int tick_buffer_len = 0;
int tick_buffer_pos = 0;

volatile Uint32 audio_callback_time;

#define NUM_KEYS 32
int key_down[NUM_KEYS];

Sint8 note_keymap(SDL_Keycode key);
void callback(void * userdata, Uint8 * stream, int len);


int main(int argv, char ** argc) {
    if (argv <= 1) {
        printf("Please specify file path\n");
        return 1;
    }

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

    load_mod(argc[1], &song);

    set_playback_song(&playback, &song);

    audio_callback_time = SDL_GetTicks();
    SDL_PauseAudioDevice(device, 0); // audio devices start paused

    for (int i = 0; i < NUM_KEYS; i++)
        key_down[i] = 0;

    int running = 1;
    char inst_select[2] = {'0', '1'};
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
                if (key >= 0 && !key_down[key]) {
                    // tick time: tick_len / spec.freq
                    Event key_event = {0, {inst_select[0], inst_select[1]},
                        EFFECT_PITCH, (Uint8)(key + MIDDLE_C), EFFECT_VELOCITY, MAX_VELOCITY};
                    // TODO choose track based on selection
                    process_event(key_event, &playback, playback.tracks + 0, tick_delay);
                    key_down[key] = 1;
                } else {
                    if (event.key.keysym.sym == SDLK_EQUALS) {
                        inst_select[1]++;
                        printf("instrument %c%c\n", inst_select[0], inst_select[1]);
                    } else if (event.key.keysym.sym == SDLK_MINUS) {
                        inst_select[1]--;
                        printf("instrument %c%c\n", inst_select[0], inst_select[1]);
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                int key = note_keymap(event.key.keysym.sym);
                if (key >= 0 && key_down[key]) {
                    Event key_event = {0, {EVENT_NOTE_CUT, EVENT_NOTE_CUT}, EFFECT_NONE, 0, EFFECT_NONE, 0};
                    process_event(key_event, &playback, playback.tracks + 0, tick_delay);
                    key_down[key] = 0;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        gui(&playback);
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


void callback(void * userdata, Uint8 * stream, int len) {
    audio_callback_time = SDL_GetTicks();

    StereoFrame * frame_stream = (StereoFrame *)stream;
    int frame_stream_len = len / sizeof(StereoFrame);
    StereoFrame * frame_stream_end = frame_stream + frame_stream_len;

    while (1) {
        if (tick_buffer_pos < tick_buffer_len) {
            int write_len = tick_buffer_len - tick_buffer_pos;
            if (write_len > (frame_stream_end - frame_stream)) {
                write_len = frame_stream_end - frame_stream;
            }
            for (int i = 0; i < write_len; i++)
                *(frame_stream++) = tick_buffer[tick_buffer_pos++];
        }

        if (frame_stream >= frame_stream_end)
            break;

        tick_buffer_len = process_tick(&playback, tick_buffer);
        tick_buffer_pos = 0;
    }
}
