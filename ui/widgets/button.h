#pragma once
#include <common.h>

#include <ui/ui.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::widgets {

class Button
{
public:
    bool draw(App *app, Rect rect, glm::vec4 color=C_ACCENT);

private:
    weak_ptr<Touch> touch;
};

} // namespace
