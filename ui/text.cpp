#include "text.h"
#include "font.h"
#include <array>
#include <glad/glad.h>

namespace chromatracker::ui {

Font FONT_DEFAULT {};

void Font::initGL()
{
    // unpack to 8bpp
    std::array<uint8_t, sizeof(fontBitmap) * 16> pixels;
    for (int i = 0; i < sizeof(fontBitmap); i++) {
        uint8_t byteVal = fontBitmap[i];
        for (int bit = 0; bit < 8; bit++) {
            int index = i * 16 + bit * 2;
            pixels[index] = 0xFF; // luminance
            pixels[index + 1] = byteVal & (1<<bit) ? 0xFF : 0x00; // alpha
        }
    }

    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                 96, 48, 0,
                 GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::ivec2 drawText(string text, glm::ivec2 position, Font *font)
{
    if (!font)
        font = &FONT_DEFAULT;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glm::ivec2 charSize = fontCharDim * 2;

    glBegin(GL_QUADS);
    glm::ivec2 curPos = position;
    for (auto &c : text) {
        if (c == '\n') {
            curPos.x = position.x;
            curPos.y += charSize.y;
            continue;
        }
        int charNum = c - ' ';
        glm::ivec2 minCoord { (charNum % fontCharCount.x) * fontCharDim.x,
                                (charNum / fontCharCount.x) * fontCharDim.y };
        glm::ivec2 maxCoord = minCoord + fontCharDim;
        glm::vec2 minCoordF = (glm::vec2)minCoord / (glm::vec2)fontBitmapDim;
        glm::vec2 maxCoordF = (glm::vec2)maxCoord / (glm::vec2)fontBitmapDim;

        glm::ivec2 maxPos = curPos + charSize;

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
