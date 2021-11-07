#include "layout.h"

namespace chromatracker::ui {

const glm::vec2 TOP_LEFT {0.0f, 0.0f};
const glm::vec2 TOP_CENTER {0.5f, 0.0f};
const glm::vec2 TOP_RIGHT {1.0f, 0.0f};
const glm::vec2 CENTER_LEFT {0.0f, 0.5f};
const glm::vec2 CENTER {0.5f, 0.5f};
const glm::vec2 CENTER_RIGHT {1.0f, 0.5f};
const glm::vec2 BOTTOM_LEFT {0.0f, 1.0f};
const glm::vec2 BOTTOM_CENTER {0.5f, 1.0f};
const glm::vec2 BOTTOM_RIGHT {1.0f, 1.0f};

glm::vec2 Rect::dim() const
{
    return max - min;
}

glm::vec2 Rect::operator[](glm::vec2 ref) const
{
    return min + ref * dim();
}

glm::vec2 Rect::center() const
{
    return (*this)[CENTER];
}

bool Rect::contains(glm::vec2 point) const
{
    return point.x >= min.x && point.y >= min.y
        && point.x < max.x && point.y < max.y;
}

glm::vec2 Rect::normalized(glm::vec2 point) const
{
    return (point - min) / dim();
}

Rect Rect::from(glm::vec2 ref, glm::vec2 pos, glm::vec2 dim)
{
    glm::vec2 min = pos - dim * ref;
    return {min, min + dim};
}

} // namespace
