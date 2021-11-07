#pragma once
#include <common.h>

#include <glm/glm.hpp>

namespace chromatracker::ui {

// reference points
extern const glm::vec2 TOP_LEFT, TOP_CENTER, TOP_RIGHT;
extern const glm::vec2 CENTER_LEFT, CENTER, CENTER_RIGHT;
extern const glm::vec2 BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT;

struct Rect
{
    glm::vec2 min {0, 0}, max {0, 0};

    glm::vec2 dim() const;
    glm::vec2 operator[](glm::vec2 ref) const;
    glm::vec2 center() const;

    bool contains(glm::vec2 point) const;
    glm::vec2 normalized(glm::vec2 point) const; // reverse of operator[]

    static Rect from(glm::vec2 ref, glm::vec2 pos, glm::vec2 dim);
};

} // namespace
