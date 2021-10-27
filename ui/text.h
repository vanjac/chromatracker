#pragma once
#include <common.h>

#include <glutils.h>
#include <glm/glm.hpp>

namespace chromatracker::ui {

struct Font
{
    void initGL();

    const uint8_t *bitmap;
    glm::ivec2 bitmapDim;
    glm::ivec2 charDim;
    GLTexture texture {0};
};

extern Font FONT_DEFAULT;

glm::ivec2 drawText(string text, glm::ivec2 position,
                    const Font *font = &FONT_DEFAULT);

} // namespace
