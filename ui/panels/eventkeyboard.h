#pragma once
#include <common.h>

#include <event.h>
#include <ui/layout.h>
#include <ui/touch.h>
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

    App * const app;

    int octave {MIDDLE_OCTAVE};

    std::weak_ptr<ui::Touch> velocityTouch;
};

} // namespace
