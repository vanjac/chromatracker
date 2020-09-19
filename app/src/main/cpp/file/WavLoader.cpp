#include "WavLoader.h"
#include "../Log.h"
#include "../Units.h"

namespace chromatracker::file {

static const char FMT_CHUNK_ID[] = "fmt ";
static const char CUE_CHUNK_ID[] = "cue ";
static const char SMPL_CHUNK_ID[] = "smpl";
static const char DATA_CHUNK_ID[] = "data";

static const int FMT_CHUNK_SIZE = 16;
// 4 byte count + 2 cue points (24 bytes per cue)
static const int CUE_CHUNK_SIZE = 52;

// https://sites.google.com/site/musicgapi/technical-documents/wav-file-format
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html


WavLoader::WavLoader(const std::string &path, InstSample *sample)
: FileUtils(path), sample(sample), format(0), frame_size(4) { }

bool WavLoader::load_wav() {
    if (!file.good()) {
        LOGE("Error opening file: %s", strerror(errno));
        return false;
    }
    LOGD("reading WAV");

    char chunk_id[5] = {0,0,0,0,0};

    file.seekg(0, file.beg);
    unsigned int riff_size = read_chunk_head(chunk_id);
    if (strcmp(chunk_id, "RIFF") != 0) {
        LOGE("Invalid WAV file");
        return false;
    }
    LOGD("Size: %d", riff_size);
    long riff_start = file.tellg();

    read_fourcc(chunk_id);
    if (strcmp(chunk_id, "WAVE") != 0) {
        LOGE("Invalid WAV file");
        return false;
    }

    // initial values
    sample->name = path;
    sample->playback_mode = PlaybackMode::ONCE;
    sample->base_key = MIDDLE_C;
    sample->finetune = 0.0f;

    long pos = riff_start + 4;

    while (pos < riff_start + riff_size) {
        file.seekg(pos, file.beg);
        uint32_t chunk_size = read_chunk_head(chunk_id);
        LOGD("Chunk %s %d", chunk_id, chunk_size);

        if (chunk_size % 2 == 1)
            chunk_size ++; // chunks are always word-aligned

        if (!strcmp(chunk_id, FMT_CHUNK_ID)) {
            read_chunk_fmt();
        } else if (!strcmp(chunk_id, CUE_CHUNK_ID)) {
            read_chunk_cue();
        } else if (!strcmp(chunk_id, SMPL_CHUNK_ID)) {
            read_chunk_smpl(chunk_size);
        } else if (!strcmp(chunk_id, DATA_CHUNK_ID)) {
            read_chunk_data(chunk_size);
        }

        pos += 8 + chunk_size;
    }

    LOGD("done reading WAV");
    return true;
}


void WavLoader::read_fourcc(char *fourcc) {
    file.read(fourcc, 4);
}

uint32_t WavLoader::read_chunk_head(char *id) {
    read_fourcc(id);
    return read_uint32();
}

void WavLoader::read_chunk_fmt() {
    format = read_uint16();
    sample->wave_channels = read_uint16();
    sample->wave_frame_rate = read_uint32();
    uint32_t data_rate = read_uint32();
    frame_size = read_uint16();
    uint16_t bits_per_sample = read_uint16();
    LOGD("FMT:\nFormat: %hd\nNum channels: %d\nSample rate: %d\nData rate: %d\nBlock size: %hd\nBits per sample: %hd",
            format, sample->wave_channels, sample->wave_frame_rate,
            data_rate, frame_size, bits_per_sample);
}

void WavLoader::read_chunk_cue() {
    uint32_t num_cues = read_uint32();
    for (int i = 0; i < num_cues; i++) {
        uint32_t cue_id = read_uint32();
        file.seekg(12, file.cur);
        uint32_t frame_num = read_uint32() / frame_size;
        file.seekg(4, file.cur);
        LOGD("Cue #%d at %d", cue_id, frame_num);
    }
}

void WavLoader::read_chunk_smpl(uint32_t chunk_size) {
    file.seekg(12, file.cur);  // skip manufacturer, product, period
    sample->base_key = read_uint32();
    sample->finetune = -read_uint32() / 4294967296.0;  // tune down
    file.seekg(8, file.cur);  // skip SMPTE
    uint32_t num_loops = read_uint32();
    uint32_t extra_data = read_uint32();
    LOGD("SMPL:\nBase note: %d\nFinetune: %f\nNum loops: %d\nExtra: %d",
         sample->base_key, sample->finetune, num_loops, extra_data);
    if (num_loops == 1) {
        sample->playback_mode = PlaybackMode::LOOP;
    } else if (num_loops > 1) {
        sample->playback_mode = PlaybackMode::SUSTAIN_LOOP;
    }
    for (int i = 0; i < num_loops; i++) {
        uint32_t cue_id = read_uint32();
        uint32_t loop_type = read_uint32();
        // TODO is the documentation wrong?
        // (start/end in frames instead of byte offsets)
        uint32_t start_frame = read_uint32();
        uint32_t end_frame = read_uint32() + 1;  // wav loop is inclusive
        file.seekg(8, file.cur);  // skip fraction, count
        LOGD("Loop #%d type %d from %d to %d",
                cue_id, loop_type, start_frame, end_frame);

        if (i == 0) {
            if (loop_type == 1)
                sample->loop_type = LoopType::PING_PONG;
            else
                sample->loop_type = LoopType::FORWARD;
            sample->loop_start = start_frame;
            sample->loop_end = end_frame;
        }
    }
}

void WavLoader::read_chunk_data(uint32_t chunk_size) {
    sample->wave_frames = chunk_size / frame_size;
    sample->loop_start = 0;
    sample->loop_end = sample->wave_frames;
    LOGD("Num frames: %d", sample->wave_frames);
    int sample_size = frame_size / sample->wave_channels;
    int num_samples = chunk_size / sample_size;
    sample->wave.reset(new float[num_samples]);

    SampleFormat sample_format;
    if (format == 3) {
        if (sample_size >= 8)
            sample_format = SampleFormat::F64;
        else
            sample_format = SampleFormat::F32;
    } else {
        if (sample_size == 4)
            sample_format = SampleFormat::S32;
        else if (sample_size == 1)
            sample_format = SampleFormat::U8;
        else
            sample_format = SampleFormat::S16;
    }
    read_samples(sample->wave.get(), chunk_size, sample_format);
}

}
