#pragma once
#include <common.h>

#include <ui/ui.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::widgets {

class Spinner
{
public:
    // return if value was adjusted
    // expects a continuous operation to be performed if it returns true
    bool draw(App *app, Rect rect, int *value, int min, int max, float scale);

private:
    weak_ptr<Touch> touch;
    float partial;
};

} // namespace
