#pragma once
#include <common.h>

#include <song.h>
#include <ui/layout.h>
#include <ui/touch.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class TrackEdit
{
public:
    void draw(App *app, Rect rect, shared_ptr<Track> track);
};

} // namespace
 