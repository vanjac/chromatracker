#pragma once
#include <common.h>

#include <edit/undoer.hpp>
#include <event.h>
#include <ui/ui.h>
#include <SDL2/SDL_events.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class EventKeyboard
{
public:
    EventKeyboard(App *app);

    void drawPiano(Rect rect);
    void drawSampleList(Rect rect);
    void keyDown(const SDL_KeyboardEvent &e);
    void keyUp(const SDL_KeyboardEvent &e);

    void reset();
    void select(const Event &event);
    int sampleIndex(); // song must be locked

    Event selected {0, {}, MIDDLE_C, 1.0f, Event::Special::None};

private:
    int pitchKeymap(SDL_Scancode key);
    int sampleKeymap(SDL_Scancode key);
    // for convenience, redirect to EventsEdit
    void writeEvent(bool playing, const Event &event, Event::Mask mask,
                    edit::OpAction action = edit::OpAction::Instant);

    App * const app;

    int octave {MIDDLE_OCTAVE};

    weak_ptr<Touch> velocityTouch;
};

} // namespace
