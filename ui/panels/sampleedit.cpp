#include "sampleedit.h"
#include <app.h>
#include <edit/songops.h>

namespace chromatracker::ui::panels {

const float SPINNER_WIDTH = 60;
const float FADE_EXP_SCALE = 14;

SampleEdit::SampleEdit(App *app)
    : app(app)
{}

void SampleEdit::draw(Rect rect)
{
    app->scissorRect(rect);

    auto sample = app->eventKeyboard.selected.sample.lock();
    if (!sample)
        return;

    float vol, fineTune, fadeOut;
    int transpose;
    {
        std::shared_lock sampleLock(sample->mu);
        vol = amplitudeToVelocity(sample->volume);
        transpose = glm::floor(sample->tune + 0.5f);
        fineTune = sample->tune - transpose;
        fadeOut = glm::log2(sample->fadeOut) / FADE_EXP_SCALE + 1;
    }

    const float lineHeight = FONT_DEFAULT.lineHeight;

    Rect volumeR = Rect::from(TL, rect(TL), {rect.dim().x, lineHeight});
    if (volumeSlider.draw(app, volumeR, &vol)) {
        app->doOperation(edit::ops::SetSampleVolume(
                         sample, velocityToAmplitude(vol)), true);
    }
    drawText("Volume", volumeR(TL), C_WHITE);

    Rect textR = drawText("Transpose: ", volumeR(BL), C_WHITE);
    Rect transposeR = Rect::from(TL, textR(TR), {SPINNER_WIDTH, lineHeight});
    if (transposeSpinner.draw(app, transposeR, &transpose,
                              -MAX_PITCH, MAX_PITCH, 1.0/10)) {
        app->doOperation(edit::ops::SetSampleTune(
                         sample, transpose + fineTune), true);
    }

    Rect fineTuneR {transposeR(TR, {20, 0}),
                    {rect.right(), transposeR.bottom()}};
    // slider wraps around at the edges. "it's a feature"
    if (fineTuneSlider.draw(app, fineTuneR, &fineTune, -0.51, 0.51)) {
        app->doOperation(edit::ops::SetSampleTune(
                         sample, transpose + fineTune), true);
    }
    drawText("Finetune", fineTuneR(TL), C_WHITE);

    // TODO exponential curve
    Rect fadeOutR = Rect::from(TL, textR(BL), {rect.dim().x, lineHeight});
    if (fadeOutSlider.draw(app, fadeOutR, &fadeOut)) {
        app->doOperation(edit::ops::SetSampleFadeOut(
            sample, glm::exp2(FADE_EXP_SCALE * (fadeOut - 1))), true);
    }
    drawText("Fade out", fadeOutR(TL), C_WHITE);
}

} // namespace
