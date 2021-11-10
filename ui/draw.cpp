#include "draw.h"
#include <glad/glad.h>

namespace chromatracker::ui {

void drawRect(ui::Rect rect, glm::vec4 color)
{
    if (color.a == 1) {
        glDisable(GL_BLEND);
        glColor3f(color.r, color.g, color.b);
    } else {
        glEnable(GL_BLEND);
        glColor4f(color.r, color.g, color.b, color.a);
    }

    float vertices[] = {rect.min.x, rect.min.y,
                        rect.min.x, rect.max.y,
                        rect.max.x, rect.max.y,
                        rect.max.x, rect.min.y};
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

} // namespace
