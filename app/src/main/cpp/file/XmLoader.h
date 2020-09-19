#ifndef CHROMATRACKER_XMLOADER_H
#define CHROMATRACKER_XMLOADER_H

#include <string>
#include "XiLoader.h"
#include "../Song.h"

namespace chromatracker::file {

class XmLoader : public XiLoader {
public:
    XmLoader(const std::string &path, Song *song);
    bool load_xm();

private:
    void read_pattern(Pattern ** track_patterns, int num_tracks,
                      Instrument ** instruments, int num_instruments,
                      uint16_t &speed);

    Song *const song;
};

}

#endif //CHROMATRACKER_XMLOADER_H
