#ifndef CHROMATRACKER_WAVLOADER_H
#define CHROMATRACKER_WAVLOADER_H

#include "FileUtils.h"
#include "../InstSample.h"

namespace chromatracker::file {

class WavLoader : public FileUtils {
public:
    WavLoader(const std::string &path, InstSample *sample);
    bool load_wav();

private:
    void read_fourcc(char *fourcc);
    // return length
    uint32_t read_chunk_head(char *id);
    void read_chunk_fmt();
    void read_chunk_cue();
    void read_chunk_smpl(uint32_t chunk_size);
    void read_chunk_data(uint32_t chunk_size);

    InstSample *const sample;
    uint16_t format;
    uint16_t frame_size;
};

}

#endif //CHROMATRACKER_WAVLOADER_H
