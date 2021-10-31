#include "song.h"

namespace chromatracker {

const int Section::NO_TEMPO = -1;
const int Section::NO_METER = -1;

void Song::clear()
{
    for (auto &sample : samples) {
        sample->deleted = true;
    }
    samples.clear();
    for (auto &track : tracks) {
        track->deleted = true;
    }
    tracks.clear();
    for (auto &section : sections) {
        section->deleted = true;
    }
    sections.clear();
    volume = 0.5;
}

} // namespace
