#ifndef CHROMATRACKER_XILOADER_H
#define CHROMATRACKER_XILOADER_H

#include "FileUtils.h"
#include "../Instrument.h"
#include "../InstSample.h"

namespace chromatracker::file {

class XiLoader : public FileUtils {
public:
    XiLoader(const std::string &path, Instrument *instrument);
    bool load_xi();

protected:
    void read_inst_data(Instrument *instrument, bool xm, size_t xm_end_header);
    const int NUM_KEYS = 96;
    const int KEY_OFFSET = 12;  // XM middle C is C-4
    const int TICK_TIME = 8;  // 8 chroma ticks per mod tick

private:
    Instrument *const instrument;
};

}

#endif //CHROMATRACKER_XILOADER_H
