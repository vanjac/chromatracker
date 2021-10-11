#pragma once
#include <common.h>

#include <glutils.h>
#include <glm/glm.hpp>

namespace chromatracker::ui {

class TextRender
{
public:
    void initGL();
    glm::ivec2 drawText(string text, glm::ivec2 position);

private:
    GLTexture fontTexture;
};

} // namespace
