#include "text.h"
#include "font.h"
#include <array>
#include <glad/glad.h>

namespace chromatracker::ui {

Font FONT_DEFAULT {FONT_DEFAULT_BITMAP, {96, 48}, {6, 8}};

void Font::initGL()
{
    // unpack 1bpp bitmap to 8bpp texture
    int bitmapSize = bitmapDim.x * bitmapDim.y;
    std::vector<uint8_t> pixels;
    pixels.reserve(bitmapSize * 2);
    for (int i = 0; i < bitmapSize / 8; i++) {
        uint8_t byteVal = bitmap[i];
        for (int bit = 0; bit < 8; bit++) {
            pixels.push_back(0xFF); // luminance
            pixels.push_back(byteVal & (1<<bit) ? 0xFF : 0x00); // alpha
        }
    }

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                 bitmapDim.x, bitmapDim.y, 0,
                 GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::ivec2 drawText(string text, glm::vec2 position, const Font *font)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glm::vec2 charSize = font->charDim * 2;
    glm::ivec2 charCount = font->bitmapDim / font->charDim;

    glBegin(GL_QUADS);
    glm::vec2 curPos = position;
    for (auto &c : text) {
        if (c == '\n') {
            curPos.x = position.x;
            curPos.y += charSize.y;
            continue;
        }
        int charNum = c - ' ';
        glm::ivec2 minCoord { (charNum % charCount.x) * font->charDim.x,
                                (charNum / charCount.x) * font->charDim.y };
        glm::ivec2 maxCoord = minCoord + font->charDim;
        glm::vec2 minCoordF = (glm::vec2)minCoord / (glm::vec2)font->bitmapDim;
        glm::vec2 maxCoordF = (glm::vec2)maxCoord / (glm::vec2)font->bitmapDim;

        glm::vec2 maxPos = curPos + charSize;

        glTexCoord2f(minCoordF.x, minCoordF.y);
        glVertex2i(curPos.x, curPos.y);
        glTexCoord2f(minCoordF.x, maxCoordF.y);
        glVertex2i(curPos.x, maxPos.y);
        glTexCoord2f(maxCoordF.x, maxCoordF.y);
        glVertex2i(maxPos.x, maxPos.y);
        glTexCoord2f(maxCoordF.x, minCoordF.y);
        glVertex2i(maxPos.x, curPos.y);

        curPos.x += charSize.x;
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);

    return curPos;
}

} // namespace
