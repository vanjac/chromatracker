#include "event.h"

namespace chromatracker {

const int Event::NO_PITCH = -1;
const float Event::NO_VELOCITY = -1.0f;

bool Event::empty()
{
    return sample.expired() && pitch == NO_PITCH && velocity == NO_VELOCITY
        && special == Special::None;
}

} // namespace
