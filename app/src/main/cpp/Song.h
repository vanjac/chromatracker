#ifndef CHROMATRACKER_SONG_H
#define CHROMATRACKER_SONG_H

#include <string>
#include <vector>
#include <list>
#include "Pattern.h"

namespace chromatracker {

const int TEMPO_NONE = -1;
const int METER_NONE = -1;

struct Page {
    Page();

    int length;  // ticks
    std::vector<Pattern *> track_patterns;
    int tempo;
    int meter;
    std::string comment;
};

struct Track {
    Track();

    std::string name;
    // elements referenced by Page and TrackPlayback
    std::list<Pattern> patterns;
    bool mute;
};

struct Song {
    Song();

    float master_volume;

    // elements referenced by NoteEventData and InstrumentPlayback
    std::list<Instrument> instruments;
    // order is important for pages
    std::vector<Track> tracks;
    // elements referenced by SongPlayback
    std::list<Page> pages;
};

}

#endif //CHROMATRACKER_SONG_H
