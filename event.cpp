#include "event.h"

namespace chromatracker {

const int Event::NO_PITCH = -1;
const float Event::NO_VELOCITY = -1.0f;

bool Event::empty() const
{
    return !sample.lock() && pitch == NO_PITCH && velocity == NO_VELOCITY
        && special == Special::None;
}

void Event::merge(const Event &other)
{
    if (auto sampleP = other.sample.lock())
        sample = sampleP;
    if (other.pitch != NO_PITCH)
        pitch = other.pitch;
    if (other.velocity != NO_VELOCITY)
        velocity = other.velocity;
    if (other.special != Special::None)
        special = other.special;
}

void Event::merge(const Event &other, Mask mask)
{
    if (mask & SAMPLE)
        sample = other.sample;
    if (mask & PITCH)
        pitch = other.pitch;
    if (mask & VELOCITY)
        velocity = other.velocity;
    if (mask & SPECIAL)
        special = other.special;
}

} // namespace
