#include "sectionedit.h"
#include <app.h>

namespace chromatracker::ui::panels {

const float SPINNER_WIDTH = 50;

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
    if (tempo != Section::NO_TEMPO) {
        if (tempoSpinner.draw(app, tempoR, &tempo, 20, 999, 1.0/2)) {
            app->doOperation(edit::ops::SetSectionTempo(section, tempo), true);
        }
    } else {
        drawText(" X ", tempoR(TL), C_WHITE)(TR);
    }

    textPos = drawText("  Meter: ", tempoR(TR), C_WHITE)(TR);
    Rect meterR = Rect::from(TL, textPos, {SPINNER_WIDTH, rect.dim().y});
    if (meter != Section::NO_METER) {
        if (meterSpinner.draw(app, meterR, &meter, 1, 32, 1.0/15)) {
            app->doOperation(edit::ops::SetSectionMeter(section, meter), true);
        }
    } else {
        drawText(" X ", meterR(TL), C_WHITE)(TR);
    }
}

} // namespace
