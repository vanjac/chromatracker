#pragma once
#include <common.h>

#include "edit/editor.h"
#include "play/songplay.h"
#include "ui/panels/browser.h"
#include "ui/panels/eventkeyboard.h"
#include "ui/panels/eventsedit.h"
#include "ui/panels/sampleedit.h"
#include "ui/panels/songedit.h"
#include "ui/settings.h"
#include "ui/ui.h"
#include <atomic>
#include <unordered_map>
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

    // general panel api
    void scissorRect(ui::Rect rect) const;
    shared_ptr<ui::Touch> captureTouch(const ui::Rect &r);

    // return if playing
    bool jamEvent(play::JamEvent jam, uint32_t timestamp);
    bool jamEvent(const SDL_KeyboardEvent &e, const Event &jam);

    edit::Editor editor;
    play::SongPlay player;
    ui::Settings settings;

private:
    enum class Tab
    {
        Events, Sample
    };

    void resizeWindow(int w, int h);

    void keyDown(const SDL_KeyboardEvent &e);

    std::shared_ptr<ui::Touch> findTouch(int id);

    ticks calcTickDelay(uint32_t timestamp); // player must be locked

    SDL_Window *window;
    ui::Rect winR {{0, 0}, {0, 0}};
    SDL_AudioDeviceID audioDevice;

    Tab tab {Tab::Events};
    ui::panels::SongEdit songEdit;
    ui::panels::SampleEdit sampleEdit;
    ui::panels::EventsEdit eventsEdit;
    ui::panels::EventKeyboard eventKeyboard;
    unique_ptr<ui::panels::Browser> browser;

    std::unordered_map<int, shared_ptr<ui::Touch>> uncapturedTouches;
    std::unordered_map<int, shared_ptr<ui::Touch>> capturedTouches;

    float tickBuffer[MAX_TICK_FRAMES * NUM_CHANNELS];
    int tickBufferLen {0}; // in SAMPLES (not frames!)
    int tickBufferPos {0};
    std::atomic<uint32_t> audioCallbackTime {0};
};

} // namespace
