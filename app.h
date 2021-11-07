#pragma once
#include <common.h>

#include "edit/operation.h"
#include "edit/songops.h"
#include "file/types.h"
#include "play/songplay.h"
#include "ui/layout.h"
#include "ui/panels/browser.h"
#include <atomic>
#include <SDL2/SDL.h>

namespace chromatracker {

const int NUM_CHANNELS = 2;
const int OUT_FRAME_RATE = 48000;
const int MAX_TICK_FRAMES = 1024; // enough for 15 BPM at 48000Hz

class App
{
public:
    App(SDL_Window *window);
    ~App();

    void main(const vector<string> args);

    void audioCallback(uint8_t *stream, int len);

    void scissorRect(ui::Rect rect) const;

private:
    void resizeWindow(int w, int h);

    void drawInfo(ui::Rect rect);
    void drawTracks(ui::Rect rect);
    void drawEvents(ui::Rect rect, Cursor playCur);
    void drawSampleList(ui::Rect rect);
    void drawPiano(ui::Rect rect);

    void keyDown(const SDL_KeyboardEvent &e);
    void keyDownEvents(const SDL_KeyboardEvent &e);
    void keyUpEvents(const SDL_KeyboardEvent &e);

    void doOperation(unique_ptr<edit::SongOp> op);

    void snapToGrid();
    void nextCell();
    void prevCell();

    ticks calcTickDelay(uint32_t timestamp); // make sure player is locked
    int pitchKeymap(SDL_Scancode key);
    int sampleKeymap(SDL_Scancode key);

    SDL_Window *window;
    ui::Rect winR {{0, 0}, {0, 0}};
    SDL_AudioDeviceID audioDevice;

    Song song;
    play::SongPlay player;

    TrackCursor editCur;
    ticks cellSize {TICKS_PER_BEAT / 4};
    int selectedSample {0};
    int selectedOctave {MIDDLE_OCTAVE};
    int selectedPitch {MIDDLE_C};

    // mode
    bool followPlayback {true};
    bool overwrite {true};
    bool record {false};

    // main loop flags
    bool movedEditCur {false}; // TODO replace with accumulator to move play cur

    unique_ptr<ui::panels::Browser> browser;

    vector<unique_ptr<edit::SongOp>> undoStack;
    vector<unique_ptr<edit::SongOp>> redoStack;

    unique_ptr<edit::ops::SetSongVolume> songVolumeOp;

    float tickBuffer[MAX_TICK_FRAMES * NUM_CHANNELS];
    int tickBufferLen {0}; // in SAMPLES (not frames!)
    int tickBufferPos {0};
    std::atomic<uint32_t> audioCallbackTime {0};
};

} // namespace
