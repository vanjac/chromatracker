#pragma once
#include <common.h>

#include <glm/glm.hpp>

namespace chromatracker::ui {

const glm::vec4 C_WHITE         {1, 1, 1, 1};
const glm::vec4 C_BLACK         {0, 0, 0, 1};
const glm::vec4 C_DARK_GRAY     {0.2, 0.2, 0.2, 1};
const glm::vec4 C_ACCENT        {0, 0.7, 0, 1};
const glm::vec4 C_ACCENT_LIGHT  {0.7, 1.0, 0.7, 1};

// multiply colors to show selected state
const glm::vec4 NORMAL_COLOR {1, 1, 1, 1};
const glm::vec4 SELECT_COLOR {1.2, 1.2, 1.2, 1};

} // namespace
