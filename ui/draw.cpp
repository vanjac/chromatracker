#include "draw.h"
#include <glad/glad.h>

namespace chromatracker::ui {

void drawRect(ui::Rect rect)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(rect.min.x, rect.min.y);
    glVertex2f(rect.min.x, rect.max.y);
    glVertex2f(rect.max.x, rect.max.y);
    glVertex2f(rect.max.x, rect.min.y);
    glEnd();
}

} // namespace
