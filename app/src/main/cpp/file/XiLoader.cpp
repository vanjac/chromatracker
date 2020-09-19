#include "XiLoader.h"
#include "../Log.h"
#include "../Units.h"
#include "../Modulation.h"

namespace chromatracker::file {

// https://github.com/milkytracker/MilkyTracker/blob/master/src/milkyplay/XIInstrument.cpp

static const int XM_C5_RATE = 8363;

XiLoader::XiLoader(const std::string &path, Instrument *instrument)
: FileUtils(path), instrument(instrument) { }


bool XiLoader::load_xi() {
    if (!file.good()) {
        LOGE("Error opening file: %s", strerror(errno));
        return false;
    }
    LOGD("reading XI");

    std::string sig = read_string(21);
    if (sig != "Extended Instrument: ") {
        LOGE("bad header");
        return false;
    }

    instrument->name = read_string(22);

    if (read_uint8() != 0x1A) {
        LOGE("no 1a");
        return false;
    }

    std::string tracker = read_string(20);
    LOGD("Tracker: %s", tracker.c_str());

    if (read_uint16() != 0x102) {     // version
        LOGE("bad version");
        return false;
    }

    read_inst_data(instrument, false, 0);

    LOGD("done reading XI");
    return true;
}

void XiLoader::read_inst_data(chromatracker::Instrument *instrument,
        bool xm, size_t xm_end_header) {
    uint16_t num_samples;
    if (xm) {
        num_samples = read_uint16();
        LOGD("Num samples: %d", num_samples);
        uint32_t sample_header_size = read_uint32();
        if (num_samples == 0)
            return;
    }
    instrument->samples.clear();

    // default values
    instrument->new_note_action = NewNoteAction::CUT;
    instrument->sample_overlap_mode = SampleOverlapMode::MIX;
    instrument->random_delay = 0;
    instrument->volume = 1.0f;
    instrument->panning = 0.0f;
    instrument->transpose = 0;
    instrument->finetune = 0.0f;
    instrument->glide = 0.0f;
    instrument->volume_adsr = ADSR();


    uint8_t sample_keymap[NUM_KEYS];
    // read note instrument table
    file.read(reinterpret_cast<char *>(sample_keymap), NUM_KEYS);
    for (int k = 0; k < NUM_KEYS; k++) {
        if (sample_keymap[k] > 15)
            sample_keymap[k] = 15;
    }

    file.seekg(48, file.cur);  // skip vol envelope
    file.seekg(48, file.cur);  // skip pan envelope
    file.seekg(8, file.cur);  // skip envelope properties

    uint8_t vol_env_type = read_uint8();
    uint8_t pan_env_type = read_uint8();

    uint8_t vibtype = read_uint8();
    uint8_t vibsweep = read_uint8();
    uint8_t vibdepth = read_uint8() * 2;
    uint8_t vibrate = read_uint8();
    LOGD("Vibrato type: %hhd sweep: %hhd depth: %hhd rate: %hhd",
         vibtype, vibsweep, vibdepth, vibrate);

    uint16_t volfade = read_uint16();
    LOGD("Vol fade: %hd", volfade);
    if (vol_env_type & 1u) {
        // only applies if envelope is enabled
        // TODO: 32768 or 65536???
        instrument->volume_adsr.release = TICK_TIME * 32768 / volfade;
    }

    uint16_t reserved = read_uint16();

    if (!xm) {  // XI
        file.seekg(20, file.cur);  // skip extra

        num_samples = read_uint16();
        if (num_samples > 16)
            num_samples = 16;
        LOGD("Num samples: %d", num_samples);
    } else {  // XM
        file.seekg(xm_end_header, file.beg);
    }

    InstSample *samples[num_samples];  // allows indexing
    bool sample16bit[num_samples];

    // read sample infos
    for (int k = 0; k < num_samples; k++)
    {
        instrument->samples.emplace_back();
        InstSample *sample = &instrument->samples.back();
        samples[k] = sample;
        sample->wave_channels = 1;  // always mono
        sample->wave_frame_rate = XM_C5_RATE;
        sample->key_start = -1;
        sample->key_end = -1;

        sample->wave_frames = read_uint32();
        sample->loop_start = read_uint32();
        sample->loop_end = sample->loop_start + read_uint32();
        sample->volume = amplitude_to_volume_control(
                static_cast<float>(read_uint8()) / 64.0f);
        auto finetune = static_cast<int8_t>(read_uint8());  // signed
        sample->finetune = static_cast<float>(finetune) / 128.0f;

        uint8_t flags = read_uint8();
        LOGD("Flags: %hhd", flags);
        switch (flags & 0x3u) {
            case 1:
                sample->playback_mode = PlaybackMode::LOOP;
                sample->loop_type = LoopType::FORWARD;
                break;
            case 2:
                sample->playback_mode = PlaybackMode::LOOP;
                sample->loop_type = LoopType::PING_PONG;
                break;
            case 0:
            default:
                sample->playback_mode = PlaybackMode::ONCE;
                sample->loop_type = LoopType::FORWARD;
                sample->loop_start = 0;
                sample->loop_end = sample->wave_frames;
                break;
        }
        sample16bit[k] = (flags & 0x10u) != 0;
        if (sample16bit[k]) {
            sample->wave_frames /= 2;
            sample->loop_start /= 2;
            sample->loop_end /= 2;
        }

        sample->panning = static_cast<float>(read_uint8()) / 127.0f - 1.0f;
        auto rel_note = static_cast<int8_t>(read_uint8());  // signed
        sample->base_key = MIDDLE_C - rel_note;
        uint16_t sample_reserved = read_uint8();
        if (sample_reserved == 0xAD) {
            LOGW("ADPCM!!");
        }
        sample->name = read_string(22);
        LOGD("Sample: %s", sample->name.c_str());
    }

    // assign keymap
    for (int key = 0; key < NUM_KEYS; key++) {
        int sample_num = sample_keymap[key];
        if (sample_num >= num_samples)
            continue;
        InstSample *sample = samples[sample_num];
        if (sample->key_start == -1)
            sample->key_start = key + KEY_OFFSET;
        sample->key_end = key + KEY_OFFSET;
    }

    // read samples
    for (int k = 0; k < num_samples; k++)
    {
        InstSample *sample = samples[k];
        if (sample->wave_frames == 0) {
            sample->wave.reset(new float[0]);
            continue;
        }

        // always mono
        int wave_samples = sample->wave_frames * sample->wave_channels;
        sample->wave.reset(new float[wave_samples]);

        if (sample16bit[k]) {
            LOGD("16 bit");
            read_samples(sample->wave.get(), wave_samples * 2,
                    SampleFormat::S16_DELTA);
        } else {
            LOGD("8 bit");
            read_samples(sample->wave.get(), wave_samples,
                    SampleFormat::S8_DELTA);
        }
    }

    // done with samples, now remove the empty ones
    for (auto itr = instrument->samples.begin();
            itr != instrument->samples.end();) {
        if (itr->wave_frames == 0) {
            itr = instrument->samples.erase(itr);
        } else {
            ++itr;
        }
    }
}

}
