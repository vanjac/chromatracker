#pragma once
#include <common.h>

#include <ui/ui.h>

namespace chromatracker { class App; }
namespace chromatracker::edit { class Editor; }

namespace chromatracker::ui::widgets {

class Slider
{
public:
    // return if value was adjusted
    // expects a continuous operation to be performed if it returns true
    bool draw(App *app, edit::Editor *editor, Rect rect, float *value,
              float min=0, float max=1, glm::vec4 color=C_ACCENT);

private:
    weak_ptr<Touch> touch;
};

} // namespace
