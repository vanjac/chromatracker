#include "trackedit.h"
#include <ui/draw.h>
#include <ui/text.h>
#include <ui/theme.h>

namespace chromatracker::ui::panels {

void TrackEdit::draw(App *app, Rect rect, shared_ptr<Track> track)
{
    std::shared_lock trackLock(track->mu);
    drawRect(rect, C_DARK_GRAY);
    if (!track->mute) {
        float vol = amplitudeToVelocity(track->volume);
        drawRect({rect(TL), rect({vol, 0.5})}, C_ACCENT);
        drawRect({rect(CC), rect({track->pan / 2 + 0.5, 1})},
                    C_ACCENT);

        drawRect(Rect::vLine(rect(CC), rect.bottom(), 1), C_WHITE);
    }
}

} // namespace
