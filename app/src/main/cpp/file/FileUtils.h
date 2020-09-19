#ifndef CHROMATRACKER_FILEUTILS_H
#define CHROMATRACKER_FILEUTILS_H

#include <string>
#include <iostream>
#include <fstream>

namespace chromatracker::file {

enum class SampleFormat {
    U8, S16, S32, F32, F64,  // WAV
    S8_DELTA, S16_DELTA  // XI
};

class FileUtils {
public:
    FileUtils(const std::string &path);

protected:
    // little endian
    uint8_t read_uint8();

    uint16_t read_uint16();

    uint32_t read_uint32();

    std::string read_string(int len);

    void read_samples(float *wave, int num_bytes, SampleFormat format);

    std::string path;
    std::ifstream file;
};

}

#endif //CHROMATRACKER_FILEUTILS_H
