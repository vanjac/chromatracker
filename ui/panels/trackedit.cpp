#include "trackedit.h"
#include <app.h>
#include <edit/songops.h>

namespace chromatracker::ui::panels {

const glm::vec4 C_MUTED_ACCENT {0.5, 0.5, 0.5, 1};

void TrackEdit::draw(App *app, Rect rect, shared_ptr<Track> track)
{
    drawRect(rect, C_DARK_GRAY);

    float vol, pan;
    bool mute;
    {
        std::shared_lock lock(track->mu);
        vol = amplitudeToVelocity(track->volume);
        pan = track->pan;
        mute = track->mute;
    }
    glm::vec4 accent = mute ? C_MUTED_ACCENT : C_ACCENT;

    Rect volumeR {rect(TL), rect(CR)};
    if (auto act = volumeSlider.draw(app, volumeR, &vol, 0, 1, accent);
            (bool)act) {
        app->undoer.doOp(edit::ops::SetTrackVolume(
                         track, velocityToAmplitude(vol)), act);
    }

    Rect panR {rect(CL), rect(BR)};
    if (auto act = panSlider.draw(app, panR, &pan, -1, 1, accent); (bool)act) {
        app->undoer.doOp(edit::ops::SetTrackPan(track, pan), act);
    }
}

} // namespace
