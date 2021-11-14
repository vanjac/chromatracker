#pragma once
#include <common.h>

#include <song.h>
#include <ui/ui.h>
#include <ui/widgets/slider.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class TrackEdit
{
public:
    void draw(App *app, Rect rect, shared_ptr<Track> track);

private:
    widgets::Slider volumeSlider, panSlider;
};

} // namespace
 