#pragma once
#include <common.h>

#include <glm/glm.hpp>

namespace chromatracker::ui {

// reference points: Top/Center/Bottom, Left/Center/Right
extern const glm::vec2 TL, TC, TR;
extern const glm::vec2 CL, CC, CR;
extern const glm::vec2 BL, BC, BR;

struct Rect
{
    glm::vec2 min {0, 0}, max {0, 0};

    glm::vec2 dim() const;
    glm::vec2 operator()(glm::vec2 ref) const;
    glm::vec2 operator()(glm::vec2 ref, glm::vec2 offset) const;
    float left() const;
    float right() const;
    float top() const;
    float bottom() const;

    bool contains(glm::vec2 point) const;
    glm::vec2 normalized(glm::vec2 point) const; // reverse of operator[]

    static Rect from(glm::vec2 ref, glm::vec2 pos, glm::vec2 dim);
    static Rect hLine(glm::vec2 min, float maxX, float width);
    static Rect vLine(glm::vec2 min, float maxY, float width);
};

} // namespace
