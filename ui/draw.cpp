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

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(rect.min.x, rect.min.y);
    glVertex2f(rect.min.x, rect.max.y);
    glVertex2f(rect.max.x, rect.max.y);
    glVertex2f(rect.max.x, rect.min.y);
    glEnd();
}

} // namespace
