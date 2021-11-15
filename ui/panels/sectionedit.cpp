#include "sectionedit.h"
#include <app.h>
#include <edit/songops.h>

namespace chromatracker::ui::panels {

const float SPINNER_WIDTH = 60;
const float CLEAR_WIDTH = 12;

void SectionEdit::draw(App *app, Rect rect, shared_ptr<Section> section)
{
    glm::vec2 textPos = rect(TL);

    int tempo, meter;
    {
        std::shared_lock lock(section->mu);
        tempo = section->tempo;
        meter = section->meter;
        
        if (section->title.empty()) {
            textPos.x += 200;
        } else {
            textPos = drawText(section->title, textPos, C_WHITE)(TR);
        }
    }

    textPos = drawText("  Tempo: ", textPos, C_WHITE)(TR);
    Rect tempoR = Rect::from(TL, textPos, {SPINNER_WIDTH, rect.dim().y});
    Rect clearR = Rect::from(TL, tempoR(TR), {CLEAR_WIDTH, rect.dim().y});
    if (tempo != Section::NO_TEMPO) {
        if (tempoSpinner.draw(app, tempoR, &tempo, 20, 999, 1.0/2)) {
            app->undoer.doOp(edit::ops::SetSectionTempo(section, tempo), true);
        }
        if (tempoButton.draw(app, clearR, C_DARK_GRAY)) {
            app->undoer.doOp(edit::ops::SetSectionTempo(
                section, Section::NO_TEMPO), true);
        }
        drawText("X", clearR(TL), C_WHITE)(TR);
    } else {
        if (tempoButton.draw(app, tempoR, C_DARK_GRAY)) {
            app->undoer.doOp(edit::ops::SetSectionTempo(section, 125), true);
        }
        drawText("(set)", tempoR(TL), C_WHITE)(TR);
    }

    textPos = drawText("  Meter: ", clearR(TR), C_WHITE)(TR);
    Rect meterR = Rect::from(TL, textPos, {SPINNER_WIDTH, rect.dim().y});
    clearR = Rect::from(TL, meterR(TR), {CLEAR_WIDTH, rect.dim().y});
    if (meter != Section::NO_METER) {
        if (meterSpinner.draw(app, meterR, &meter, 1, 32, 1.0/15)) {
            app->undoer.doOp(edit::ops::SetSectionMeter(section, meter), true);
        }
        if (meterButton.draw(app, clearR, C_DARK_GRAY)) {
            app->undoer.doOp(edit::ops::SetSectionMeter(
                section, Section::NO_METER), true);
        }
        drawText("X", clearR(TL), C_WHITE)(TR);
    } else {
        if (meterButton.draw(app, meterR, C_DARK_GRAY)) {
            app->undoer.doOp(edit::ops::SetSectionMeter(section, 4), true);
        }
        drawText("(set)", meterR(TL), C_WHITE)(TR);
    }
}

} // namespace
