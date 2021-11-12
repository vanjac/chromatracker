#include "trackedit.h"
#include <app.h>
#include <ui/draw.h>
#include <ui/text.h>
#include <ui/theme.h>

namespace chromatracker::ui::panels {

void TrackEdit::draw(App *app, Rect rect, shared_ptr<Track> track)
{
    drawRect(rect, C_DARK_GRAY);

    float vol, pan;
    bool mute;
    {
        std::shared_lock trackLock(track->mu);
        vol = amplitudeToVelocity(track->volume);
        pan = track->pan;
        mute = track->mute;
    }
    glm::vec4 color = mute ? glm::vec4(0.5, 0.5, 0.5, 1) : C_ACCENT;

    Rect volumeR {rect(TL), rect(CR)};
    if (volumeTouch.expired())
        volumeTouch = app->captureTouch(volumeR);
    auto touch = volumeTouch.lock();
    if (touch) {
        for (auto &event : touch->events) {
            if (event.type == SDL_MOUSEMOTION) {
                vol += (float)event.motion.xrel / volumeR.dim().x;
                vol = glm::clamp(vol, 0.0f, 1.0f);
                app->doOperation(edit::ops::SetTrackVolume(
                    track, velocityToAmplitude(vol)), true);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                app->endContinuous();
            }
        }
        touch->events.clear();
    }
    drawRect({volumeR(TL), volumeR({vol, 1})},
             color * (touch ? SELECT_COLOR : NORMAL_COLOR));

    Rect panR {rect(CL), rect(BR)};
    if (panTouch.expired())
        panTouch = app->captureTouch(panR);
    touch = panTouch.lock();
    if (touch) {
        for (auto &event : touch->events) {
            if (event.type == SDL_MOUSEMOTION) {
                pan += 2.0f * (float)event.motion.xrel / panR.dim().x;
                pan = glm::clamp(pan, -1.0f, 1.0f);
                app->doOperation(edit::ops::SetTrackPan(track, pan), true);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                app->endContinuous();
            }
        }
        touch->events.clear();
    }
    drawRect({panR(TC), panR({pan / 2 + 0.5, 1})},
             color * (touch ? SELECT_COLOR : NORMAL_COLOR));

    drawRect(Rect::vLine(rect(CC), rect.bottom(), 1), C_WHITE);
}

} // namespace
