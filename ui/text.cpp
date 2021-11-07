#include "text.h"
#include "font.h"
#include <array>
#include <stdexcept>
#include <glad/glad.h>
#include <SDL2/SDL_filesystem.h>

namespace chromatracker::ui {

Font FONT_DEFAULT;

FT_Library library;
uint8_t bitmapBuffer[65536];

void initText()
{
    FT_Error error;
    if (error = FT_Init_FreeType(&library)) {
        throw std::runtime_error("Error initializing FreeType");
    }

    string path = string(SDL_GetBasePath()) + "Hack-Bold.ttf";
    if (error = FT_New_Face(library, path.c_str(), 0, &FONT_DEFAULT.face)) {
        throw std::runtime_error("Error loading font");
    }

    FONT_DEFAULT.charHeight = 16;
    if (error = FT_Set_Pixel_Sizes(FONT_DEFAULT.face,
                                   0, FONT_DEFAULT.charHeight)) {
        throw std::runtime_error("Error setting font size");
    }

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

    fontChar.drawOffset = glm::ivec2(slot->bitmap_left,
                                     font->charHeight - slot->bitmap_top);
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

glm::vec2 drawText(string text, glm::vec2 position, Font *font)
{
    if (text.size() == 0) {
        return position;
    }

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    // TODO iterate unicode points
    glm::vec2 curPos = position;
    for (auto &c : text) {
        if (c == '\n') {
            curPos.x = position.x;
            curPos.y += font->charHeight;
            continue;
        }
        const FontChar &fontChar = getChar(font, c);
        
        glBindTexture(GL_TEXTURE_2D, fontChar.texture);

        glm::vec2 minPos = curPos + glm::vec2(fontChar.drawOffset);
        glm::vec2 maxPos = minPos + glm::vec2(fontChar.bitmapDim);

        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 0);
        glVertex2f(minPos.x, minPos.y);
        glTexCoord2f(0, 1);
        glVertex2f(minPos.x, maxPos.y);
        glTexCoord2f(1, 1);
        glVertex2f(maxPos.x, maxPos.y);
        glTexCoord2f(1, 0);
        glVertex2f(maxPos.x, minPos.y);
        glEnd();

        curPos.x += fontChar.advanceX;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);

    return curPos;
}

} // namespace
