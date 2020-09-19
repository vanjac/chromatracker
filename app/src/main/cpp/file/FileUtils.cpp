#include "FileUtils.h"

namespace chromatracker::file {

FileUtils::FileUtils(const std::string &path)
        : path(path), file(path) {}

uint8_t FileUtils::read_uint8() {
    uint8_t v = 0;
    file.read(reinterpret_cast<char *>(&v), 1);
    return v;
}

uint16_t FileUtils::read_uint16() {
    uint16_t v = 0;
    file.read(reinterpret_cast<char *>(&v), 2);
    return v;
}

uint32_t FileUtils::read_uint32() {
    unsigned int v = 0;
    file.read(reinterpret_cast<char *>(&v), 4);
    return v;
}

std::string FileUtils::read_string(int len) {
    char buf[len + 1];
    buf[len] = 0;
    file.read(buf, len);
    // filter unknown characters
    for (int i = 0; i < sizeof(buf); i++) {
        if (buf[i] < 0 || buf[i] >= 127)
            buf[i] = '_';
    }
    return std::string(buf);
}

void FileUtils::read_samples(float *wave, int num_bytes, SampleFormat format) {
    char buffer[1024];
    unsigned bytes_read = 0;
    unsigned sample_pos = 0;
    int delta_store = 0;
    while (bytes_read < num_bytes) {
        int len = sizeof(buffer);
        if (num_bytes - bytes_read < len)
            len = num_bytes - bytes_read;
        file.read(buffer, len);
        bytes_read += len;

        switch (format) {
            case SampleFormat::U8: {
                auto u8_buffer = reinterpret_cast<uint8_t *>(buffer);
                for (int i = 0; i < len; i++) {
                    wave[sample_pos++] =
                            static_cast<float>(u8_buffer[i] - 127) / 128.0f;
                }
                break;
            }
            case SampleFormat::S16: {
                auto s16_buffer = reinterpret_cast<int16_t *>(buffer);
                for (int i = 0; i < len / 2; i++) {
                    wave[sample_pos++] =
                            static_cast<float>(s16_buffer[i]) / 32768.0f;
                }
                break;
            }
            case SampleFormat::S32: {
                auto s32_buffer = reinterpret_cast<int32_t *>(buffer);
                for (int i = 0; i < len / 4; i++) {
                    wave[sample_pos++] =
                            static_cast<float>(s32_buffer[i]) / 2147483648.0f;
                }
                break;
            }
            case SampleFormat::F32: {
                auto float_buffer = reinterpret_cast<float *>(buffer);
                for (int i = 0; i < len / 4; i++) {
                    wave[sample_pos++] = float_buffer[i];
                }
                break;
            }
            case SampleFormat::F64: {
                auto double_buffer = reinterpret_cast<double *>(buffer);
                for (int i = 0; i < len / 8; i++) {
                    wave[sample_pos++] = static_cast<float>(double_buffer[i]);
                }
                break;
            }
            case SampleFormat::S8_DELTA: {
                auto s8_buffer = reinterpret_cast<int8_t *>(buffer);
                int8_t delta = delta_store;
                for (int i = 0; i < len; i++) {
                    delta += s8_buffer[i];
                    wave[sample_pos] = static_cast<float>(delta) / 128.0f;
                    /*if (wave[sample_pos] > 1.0f)
                        wave[sample_pos] = 1.0f;
                    else if (wave[sample_pos] < -1.0f)
                        wave[sample_pos] = -1.0f;*/
                    sample_pos++;
                }
                delta_store = delta;
                break;
            }
            case SampleFormat::S16_DELTA: {
                auto s16_buffer = reinterpret_cast<int16_t *>(buffer);
                int16_t delta = delta_store;
                for (int i = 0; i < len / 2; i++) {
                    delta += s16_buffer[i];
                    wave[sample_pos] = static_cast<float>(delta) / 32768.0f;
                    /*if (wave[sample_pos] > 1.0f)
                        wave[sample_pos] = 1.0f;
                    else if (wave[sample_pos] < -1.0f)
                        wave[sample_pos] = -1.0f;*/
                    sample_pos++;
                }
                delta_store = delta;
                break;
            }
        }
    }
}

}