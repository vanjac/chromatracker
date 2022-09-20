#pragma once
#include <common.h>

#include <event.h>
#include <ui/ui.h>
#include <SDL2/SDL_events.h>

namespace chromatracker { class App; }
namespace chromatracker::edit { class Editor; }

namespace chromatracker::ui::panels {

class EventKeyboard
{
public:
    EventKeyboard(App *app);

    void drawPiano(Rect rect);
    void drawSampleList(Rect rect);
    void keyDown(const SDL_KeyboardEvent &e);
    void keyUp(const SDL_KeyboardEvent &e);

private:
    int pitchKeymap(SDL_Scancode key);
    int sampleKeymap(SDL_Scancode key);

    App * const app;
    edit::Editor * const editor;

    int octave {MIDDLE_OCTAVE};

    weak_ptr<Touch> velocityTouch;
};

} // namespace
