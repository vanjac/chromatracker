#include "button.h"
#include <app.h>

namespace chromatracker::ui::widgets {

bool Button::draw(App *app, Rect rect, glm::vec4 color)
{
    if (touch.expired())
        touch = app->captureTouch(rect);
    bool clicked = false;
    bool over = false;
    if (auto touchP = touch.lock()) {
        over = rect.contains(touchP->pos);
        for (auto &event : touchP->events) {
            if (event.type == SDL_MOUSEBUTTONUP && over) {
                clicked = true;
            }
        }
        touchP->events.clear();
    }

    drawRect(rect, color * (over ? SELECT_COLOR : NORMAL_COLOR));
    return clicked;
}

} // namespace
