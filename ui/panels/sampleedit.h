#pragma once
#include <common.h>

#include <sample.h>
#include <ui/ui.h>
#include <ui/widgets/slider.h>
#include <ui/widgets/spinner.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class SampleEdit
{
public:
    SampleEdit(App *app);

    void draw(Rect rect);

private:
    App * const app;

    widgets::Slider volumeSlider, fineTuneSlider, fadeOutSlider;
    widgets::Spinner transposeSpinner;
};

} // namespace
