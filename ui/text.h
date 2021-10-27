#pragma once
#include <common.h>

#include <glutils.h>
#include <glm/glm.hpp>

namespace chromatracker::ui {

struct Font
{
    void initGL();

    GLTexture texture;
};

extern Font FONT_DEFAULT;

glm::ivec2 drawText(string text, glm::ivec2 position, Font *font = nullptr);

} // namespace
