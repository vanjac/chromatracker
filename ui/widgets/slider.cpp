#include "slider.h"
#include <app.h>

namespace chromatracker::ui::widgets {

edit::OpAction Slider::draw(App *app, Rect rect, float *value,
                            float min, float max, glm::vec4 color)
{
    if (touch.expired())
        touch = app->captureTouch(rect);
    edit::OpAction action = edit::OpAction::None;
    auto touchP = touch.lock();
    if (touchP) {
        for (auto &event : touchP->events) {
            if (event.type == SDL_MOUSEMOTION) {
                *value += (max - min) * event.motion.xrel / rect.dim().x;
                *value = glm::clamp(*value, min, max);
                action = edit::OpAction::Continuous;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                action = edit::OpAction::EndContinuous;
            }
        }
        touchP->events.clear();
    }

    drawRect(rect, C_DARK_GRAY * (touchP ? SELECT_COLOR : NORMAL_COLOR));
    float zeroPos = -min / (max - min);
    float valuePos = (*value - min) / (max - min);
    drawRect({rect({zeroPos, 0}), rect({valuePos, 1})},
             color * (touchP ? SELECT_COLOR : NORMAL_COLOR));
    if (min != 0) {
        drawRect(Rect::vLine(rect({zeroPos, 0}), rect.bottom(), 1), C_WHITE);
    }

    return action;
}

} // namespace
