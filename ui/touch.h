#pragma once
#include <common.h>

#include <glm/glm.hpp>
#include <SDL2/SDL_events.h>

namespace chromatracker::ui {

struct Touch
{
    int id;
    bool captured;
    vector<SDL_Event> events;
    int button;
    glm::vec2 pos;
};

} // namespace
