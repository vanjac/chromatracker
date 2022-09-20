#pragma once
#include <common.h>

#include <edit/undoer.hpp>
#include <ui/ui.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::widgets {

class Slider
{
public:
    edit::OpAction draw(App *app, Rect rect, float *value,
                        float min=0, float max=1, glm::vec4 color=C_ACCENT);

private:
    weak_ptr<Touch> touch;
};

} // namespace
