#pragma once
#include <common.h>

#include "play/songplay.h"
#include "ui/text.h"
#include <SDL2/SDL.h>

namespace chromatracker {

const int NUM_CHANNELS = 2;
const int OUT_FRAME_RATE = 48000;
const int MAX_TICK_FRAMES = 1024;

class App
{
public:
    App(SDL_Window *window);
    ~App();

    void main(const vector<string> args);

    void audioCallback(uint8_t *stream, int len);

private:
    void resizeWindow(int w, int h);

    void keyDown(const SDL_KeyboardEvent &e);
    void keyUp(const SDL_KeyboardEvent &e);
    ticks calcTickDelay(uint32_t timestamp); // make sure player is locked

    int pitchKeymap(SDL_Keycode key);

    SDL_Window *window;
    int winW, winH;
    SDL_AudioDeviceID audioDevice;

    Song song;
    play::SongPlay player;

    Cursor editCursor;
    int selectedSample {0};
    int selectedOctave {MIDDLE_OCTAVE};
    int selectedTrack {0};

    // mode
    bool followPlayback {true};

    // main loop flags
    bool movedEditCursor {false};

    // enough for 15 BPM at 48000Hz
    float tickBuffer[MAX_TICK_FRAMES * NUM_CHANNELS];
    int tickBufferLen {0}; // in SAMPLES (not frames!)
    int tickBufferPos {0};
    uint32_t audioCallbackTime {0};

    ui::TextRender text;
};

} // namespace
