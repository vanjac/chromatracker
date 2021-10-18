#include "layout.h"

namespace chromatracker::ui {

float Rect::width() const
{
    return max.x - min.x;
}

float Rect::height() const
{
    return max.y - min.y;
}

glm::vec2 Rect::center() const
{
    return (max + min) / 2.0f;
}

} // namespace
