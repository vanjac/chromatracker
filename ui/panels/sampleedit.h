#pragma once
#include <common.h>

#include <sample.h>
#include <ui/ui.h>
#include <ui/widgets/slider.h>
#include <ui/widgets/spinner.h>

namespace chromatracker { class App; }
namespace chromatracker::edit { class Editor; }

namespace chromatracker::ui::panels {

class SampleEdit
{
public:
    SampleEdit(App *app);

    void draw(Rect rect);

private:
    App * const app;
    edit::Editor * const editor;

    widgets::Slider volumeSlider, fineTuneSlider, fadeOutSlider;
    widgets::Spinner transposeSpinner;
};

} // namespace
