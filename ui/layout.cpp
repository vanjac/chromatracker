#include "layout.h"

namespace chromatracker::ui {

const glm::vec2 TL {0.0f, 0.0f};
const glm::vec2 TC {0.5f, 0.0f};
const glm::vec2 TR {1.0f, 0.0f};
const glm::vec2 CL {0.0f, 0.5f};
const glm::vec2 CC {0.5f, 0.5f};
const glm::vec2 CR {1.0f, 0.5f};
const glm::vec2 BL {0.0f, 1.0f};
const glm::vec2 BC {0.5f, 1.0f};
const glm::vec2 BR {1.0f, 1.0f};

glm::vec2 Rect::dim() const
{
    return max - min;
}

glm::vec2 Rect::operator()(glm::vec2 ref) const
{
    return min + ref * dim();
}

glm::vec2 Rect::operator()(glm::vec2 ref, glm::vec2 offset) const
{
    return (*this)(ref) + offset;
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

Rect Rect::hLine(glm::vec2 min, float maxX, float width)
{
    min.y += 0.5; // TODO only for odd widths
    return {{min.x, min.y - width/2}, {maxX, min.y + width/2}};
}

Rect Rect::vLine(glm::vec2 min, float maxY, float width)
{
    min.x += 0.5;
    return {{min.x - width/2, min.y}, {min.x + width/2, maxY}};
}

} // namespace
