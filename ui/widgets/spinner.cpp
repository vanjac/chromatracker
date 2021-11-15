#include "spinner.h"

#include <app.h>

namespace chromatracker::ui::widgets {

bool Spinner::draw(App *app, Rect rect, int *value,
                   int min, int max, float scale)
{
    if (touch.expired())
        touch = app->captureTouch(rect);
    bool adjusted = false;
    auto touchP = touch.lock();
    if (touchP) {
        for (auto &event : touchP->events) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                partial = 0;
            } else if (event.type == SDL_MOUSEMOTION) {
                partial += event.motion.xrel * scale;
                if (partial >= 1) {
                    *value += (int)glm::floor(partial);
                    if (*value > max) *value = max;
                    partial -= glm::floor(partial);
                    adjusted = true;
                } else if (partial <= -1) {
                    *value -= (int)glm::floor(-partial);
                    if (*value < min) *value = min;
                    partial += glm::floor(-partial);
                    adjusted = true;
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                app->undoer.endContinuous();
            }
        }
        touchP->events.clear();
    }

    drawRect(rect, C_DARK_GRAY * (touchP ? SELECT_COLOR : NORMAL_COLOR));
    drawText(std::to_string(*value), rect(TL), C_WHITE);

    return adjusted;
}

} // namespace
