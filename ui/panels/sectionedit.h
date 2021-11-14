#pragma once
#include <common.h>

#include <song.h>
#include <ui/ui.h>
#include <ui/widgets/spinner.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class SectionEdit
{
public:
    void draw(App *app, Rect rect, shared_ptr<Section> section);

private:
    widgets::Spinner tempoSpinner, meterSpinner;
};

} // namespace
