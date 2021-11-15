#pragma once
#include <common.h>

#include <song.h>
#include <ui/ui.h>
#include <ui/widgets/slider.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class SongEdit
{
public:
    void draw(App *app, Rect rect, Song *song);

private:
    widgets::Slider volumeSlider;
};

} // namespace
