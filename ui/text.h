#pragma once
#include <common.h>

#include "layout.h"
#include <glutils.h>
#include <unordered_map>
#include <ft2build.h> // TODO could we avoid including in header?
#include FT_FREETYPE_H
#include <glm/glm.hpp>

namespace chromatracker::ui {

struct FontChar
{
    GLTexture texture {0};
    glm::ivec2 bitmapDim {0, 0};
    glm::ivec2 drawOffset {0, 0};
    float advanceX {0};
};

struct Font
{
    FT_Face face;
    std::unordered_map<unsigned, FontChar> chars;
    float lineHeight;
};

extern Font FONT_DEFAULT;

void initText();
void closeText();

Rect drawText(string text, glm::vec2 position, glm::vec4 color,
              Font *font = &FONT_DEFAULT);

} // namespace
