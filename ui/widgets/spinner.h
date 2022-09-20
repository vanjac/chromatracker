#pragma once
#include <common.h>

#include <edit/undoer.hpp>
#include <ui/ui.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::widgets {

class Spinner
{
public:
    edit::OpAction draw(App *app, Rect rect, int *value,
                        int min, int max, float scale);

private:
    weak_ptr<Touch> touch;
    float partial;
};

} // namespace
