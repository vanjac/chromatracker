#include "Song.h"

namespace chromatracker {

Page::Page() :
        length(0),
        tempo(TEMPO_NONE),
        meter(METER_NONE),
        comment("") { }

Track::Track() :
        name("Track"),
        mute(false) { }

Song::Song() :
        master_volume(1.0f) { }

}
