#include "songedit.h"
#include <app.h>
#include <edit/editor.h>
#include <edit/songops.h>

namespace chromatracker::ui::panels {

void SongEdit::draw(App *app, edit::Editor *editor, Rect rect, Song *song)
{
    app->scissorRect(rect);

    float vol;
    {
        std::shared_lock songLock(song->mu);
        vol = amplitudeToVelocity(song->volume);
    }

    if (volumeSlider.draw(app, editor, rect, &vol)) {
        editor->undoer.doOp(edit::ops::SetSongVolume(
            velocityToAmplitude(vol)), true);
    }
    drawText("Volume", rect(TL), C_WHITE);
}

} // namespace
