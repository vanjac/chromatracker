#pragma once
#include <common.h>

#include <glm/glm.hpp>

namespace chromatracker::ui {

extern const uint8_t fontBitmap[576];
const glm::ivec2 fontBitmapDim {96, 48};
const glm::ivec2 fontCharDim {6, 8};
const glm::ivec2 fontCharCount = fontBitmapDim / fontCharDim;

} // namespace
