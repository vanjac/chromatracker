#pragma once
#include <common.h>

#include <glm/glm.hpp>

namespace chromatracker::ui {

struct Rect
{
    glm::vec2 min {0, 0}, max {0, 0};

    float width() const;
    float height() const;
    glm::vec2 center() const;
};

} // namespace
