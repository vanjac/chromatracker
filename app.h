#pragma once
#include <common.h>

#include "edit/operation.h"
#include "edit/songops.h"
#include "file/types.h"
#include "play/songplay.h"
#include "ui/panels/browser.h"
#include "ui/panels/eventkeyboard.h"
#include "ui/panels/sectionedit.h"
#include "ui/panels/trackedit.h"
#include "ui/settings.h"
#include "ui/ui.h"
#include "ui/widgets/slider.h"
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
    std::shared_ptr<ui::Touch> captureTouch(const ui::Rect &r);
    void endContinuous();

    // return if playing
    bool jamEvent(play::JamEvent jam, uint32_t timestamp);
    bool jamEvent(const SDL_KeyboardEvent &e, const Event &jam);
    void writeEvent(bool playing, const Event &event, Event::Mask mask,
                    bool continuous=false);

    Song song;
    ui::Settings settings;

private:
    void resizeWindow(int w, int h);

    void drawInfo(ui::Rect rect);
    void drawTracks(ui::Rect rect);
    void drawEvents(ui::Rect rect, Cursor playCur);

    void keyDown(const SDL_KeyboardEvent &e);
    void keyDownEvents(const SDL_KeyboardEvent &e);
    void keyUpEvents(const SDL_KeyboardEvent &e);

    std::shared_ptr<ui::Touch> findTouch(int id);

    void snapToGrid();
    void nextCell();
    void prevCell();

    ticks calcTickDelay(uint32_t timestamp); // player must be locked

    SDL_Window *window;
    ui::Rect winR {{0, 0}, {0, 0}};
    SDL_AudioDeviceID audioDevice;

    play::SongPlay player;

    TrackCursor editCur;
    ticks cellSize {TICKS_PER_BEAT / 4};

    // mode
    bool followPlayback {true};

    // main loop flags
    bool movedEditCur {false}; // TODO replace with accumulator to move play cur

    ui::panels::EventKeyboard eventKeyboard;
    unique_ptr<ui::panels::Browser> browser;
    vector<ui::panels::TrackEdit> trackEdits;
    vector<ui::panels::SectionEdit> sectionEdits;

    ui::widgets::Slider songVolumeSlider;

    vector<unique_ptr<edit::SongOp>> undoStack;
    vector<unique_ptr<edit::SongOp>> redoStack;
    // should either be back of undo stack or null
    edit::SongOp *continuousOp {nullptr};

    std::unordered_map<int, shared_ptr<ui::Touch>> uncapturedTouches;
    std::unordered_map<int, shared_ptr<ui::Touch>> capturedTouches;

    float tickBuffer[MAX_TICK_FRAMES * NUM_CHANNELS];
    int tickBufferLen {0}; // in SAMPLES (not frames!)
    int tickBufferPos {0};
    std::atomic<uint32_t> audioCallbackTime {0};

public:
    // op is by value not by reference for move semantics since operations are
    // typically constructed in place
    // (I think??? is there a better way to do this? TODO)
    template<typename T>
    bool doOperation(T op)
    {
        continuousOp = nullptr;
        // remember that there will be less to copy/move before doIt is called
        // than after
        auto uniqueOp = std::make_unique<T>(std::move(op));
        if (uniqueOp->doIt(&song)) {
            undoStack.push_back(std::move(uniqueOp));
            redoStack.clear();
            return true;
        }
        return false;
    }

    template<typename T>
    void doOperation(T op, bool continuous)
    {
        if (!continuous) {
            doOperation(std::move(op));
        } else if (auto prevOp = dynamic_cast<T*>(continuousOp)) {
            prevOp->undoIt(&song);
            *prevOp = op;
            prevOp->doIt(&song);
        } else {
            if (doOperation(std::move(op)))
                continuousOp = undoStack.back().get();
        }
    }
};

} // namespace
