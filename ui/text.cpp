#include "text.h"
#include <array>
#include <stdexcept>
#include <glad/glad.h>
#include <SDL2/SDL_filesystem.h>
#include <utf8.h>

namespace chromatracker::ui {

Font FONT_DEFAULT;

FT_Library library;
uint8_t bitmapBuffer[65536];

glm::vec2 QUAD_TEX_COORDS[] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

void initText()
{
    FT_Error error;
    if (error = FT_Init_FreeType(&library)) {
        throw std::runtime_error("Error initializing FreeType");
    }

    string path = string(SDL_GetBasePath()) + "NotoSansMono-Bold.ttf";
    if (error = FT_New_Face(library, path.c_str(), 0, &FONT_DEFAULT.face)) {
        throw std::runtime_error("Error loading font");
    }

    if (error = FT_Set_Pixel_Sizes(FONT_DEFAULT.face, 0, 16)) {
        throw std::runtime_error("Error setting font size");
    }
    FONT_DEFAULT.lineHeight = FONT_DEFAULT.face->size->metrics.height / 64.0f;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);    
}

void closeText()
{
    FT_Done_Face(FONT_DEFAULT.face);
    FT_Done_FreeType(library);
}

const FontChar & getChar(Font *font, unsigned c)
{
    if (font->chars.count(c))
        return font->chars[c];
    FontChar &fontChar = font->chars[c];

    if (FT_Load_Char(font->face, c, FT_LOAD_RENDER))
        return fontChar;
    FT_GlyphSlot slot = font->face->glyph;

    fontChar.bitmapDim = glm::ivec2(slot->bitmap.width, slot->bitmap.rows);

    // TODO I think using charHeight is incorrect; measure to baseline?
    fontChar.drawOffset = glm::ivec2(slot->bitmap_left, -slot->bitmap_top);
    fontChar.advanceX = slot->advance.x / 64.0f;

    int dstI = 0;
    for (int y = 0; y < fontChar.bitmapDim.y; y++) {
        int rowI = y * slot->bitmap.pitch;
        for (int x = 0; x < fontChar.bitmapDim.x; x++) {
            bitmapBuffer[dstI++] = 0xFF; // luminance
            bitmapBuffer[dstI++] = slot->bitmap.buffer[rowI + x];
        }
    }

    glGenTextures(1, &fontChar.texture);
    glBindTexture(GL_TEXTURE_2D, fontChar.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                 fontChar.bitmapDim.x, fontChar.bitmapDim.y, 0,
                 GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, bitmapBuffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    return fontChar;
}

Rect drawText(string text, glm::vec2 position, glm::vec4 color, Font *font)
{
    if (text.size() == 0)
        return {position, {position.x, position.y + font->lineHeight}};
    FT_Size_Metrics &metrics = font->face->size->metrics;
    glm::vec2 pen = position;
    pen.y += metrics.ascender / 64.0f; // TODO ?

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, QUAD_TEX_COORDS);
    glm::vec2 vertices[4];
    glVertexPointer(2, GL_FLOAT, 0, vertices);

    glEnable(GL_BLEND);
    glColor4f(color.r, color.g, color.b, color.a);

    auto strIt = text.begin();
    while (strIt < text.end()) {
        uint32_t c;
        try {
            c = utf8::next(strIt, text.end());
        } catch (utf8::exception e) {
            break;
        }
        const FontChar &fontChar = getChar(font, c);

        glm::vec2 minPos = pen + glm::vec2(fontChar.drawOffset);
        glm::vec2 maxPos = minPos + glm::vec2(fontChar.bitmapDim);

        vertices[0] = minPos;
        vertices[1] = glm::vec2(minPos.x, maxPos.y);
        vertices[2] = maxPos;
        vertices[3] = glm::vec2(maxPos.x, minPos.y);
        glBindTexture(GL_TEXTURE_2D, fontChar.texture);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        pen.x += fontChar.advanceX;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    return {position, {pen.x, position.y + font->lineHeight}};
}

} // namespace
