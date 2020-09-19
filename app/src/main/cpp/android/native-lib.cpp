#include <jni.h>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <random>
#include <oboe/Oboe.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include "../Log.h"
#include "../Song.h"
#include "../InstSample.h"
#include "../play/InstrumentPlayback.h"
#include "../play/TrackPlayback.h"
#include "../Pattern.h"
#include "../file/WavLoader.h"
#include "../file/XiLoader.h"
#include "../file/XmLoader.h"
#include "../Units.h"
#include "../play/SongPlayback.h"

class ChromaPlayer : public oboe::AudioStreamCallback {
public:

    ChromaPlayer() :
    song_play(&song, FRAME_RATE, &random),
    jam_channel(&jam_track, &random),
    tick_buffer_pos(0),
    tick_buffer_len(0) {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(NUM_CHANNELS);
        builder.setSampleRate(FRAME_RATE);
        builder.setCallback(this);

        auto result = builder.openManagedStream(out_stream);
        if (result != oboe::Result::OK) {
            LOGE("Failed to create stream: %s", oboe::convertToText(result));
            return;
        }

        chromatracker::file::XmLoader loader("/sdcard/Download/MOD.xm", &song);
        loader.load_xm();

        std::random_device rand_dev;
        random.seed(rand_dev());

        result = out_stream->requestStart();
        if (result != oboe::Result::OK) {
            LOGE("Failed to start stream: %s", oboe::convertToText(result));
            return;
        }
    }

    void note_event(const chromatracker::NoteEventData &data) {
        if (data.pitch != chromatracker::PITCH_NONE)
            LOGD("%s", chromatracker::pitch_to_string(data.pitch).c_str());
        jam_channel.execute_event(chromatracker::Event(0, data), FRAME_RATE);
    }

    void song_start() {
        song_play.play_from_beginning();
    }

    void song_stop() {
        song_play.stop_playback();
    }

    chromatracker::Song *get_song() {
        return &song;
    }

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audio_stream, void *audio_data, int32_t num_frames) override {
        // assume float  TODO don't
        auto *output_data = reinterpret_cast<float *>(audio_data);

        int num_samples = num_frames * NUM_CHANNELS;
        int write_pos = 0;

        for (int i = 0; i < num_samples; i++)
            output_data[i] = 0.0f;

        while (true) {
            if (tick_buffer_pos < tick_buffer_len) {
                int write_len = tick_buffer_len - tick_buffer_pos;
                if (write_len > (num_samples - write_pos)) {
                    write_len = num_samples - write_pos;
                }
                for (int i = 0; i < write_len; i++)
                    output_data[write_pos++] = tick_buffer[tick_buffer_pos++];
            }

            if (write_pos >= num_samples)
                break;

            tick_buffer_len = song_play.process_tick(tick_buffer, MAX_TICK_FRAMES) * NUM_CHANNELS;
            tick_buffer_pos = 0;
        }

        jam_channel.process_tick(output_data, num_frames, FRAME_RATE, 1.0f);

        // clip!!
        for (int i = 0; i < num_samples; i++) {
            if (output_data[i] > 1.0f)
                output_data[i] = 1.0f;
            else if (output_data[i] < -1.0f)
                output_data[i] = -1.0f;
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    static int constexpr NUM_CHANNELS = oboe::ChannelCount::Stereo;
    static int constexpr FRAME_RATE = 48000;
    // enough for 15 BPM at 48000Hz
    static int constexpr MAX_TICK_FRAMES = 1024;

    oboe::ManagedStream out_stream;
    chromatracker::Song song;
    std::default_random_engine random;
    chromatracker::Track jam_track;
    chromatracker::play::TrackPlayback jam_channel;
    chromatracker::play::SongPlayback song_play;

    float tick_buffer[MAX_TICK_FRAMES * NUM_CHANNELS];
    int tick_buffer_len;  // in SAMPLES (not frames!)
    int tick_buffer_pos;
};


static ChromaPlayer *the_player = nullptr;

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_startAudio(
        JNIEnv *env,
        jobject,
        jobject view) {
    if (the_player == nullptr)
        the_player = new ChromaPlayer();
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_stopAudio(
        JNIEnv *env,
        jobject /* this */,
        jobject view) {
    if (the_player != nullptr) {
        delete the_player;
        the_player = nullptr;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_startPattern(
        JNIEnv *env,
        jobject /* this */,
        jobject view) {
    if (the_player != nullptr)
        the_player->song_start();
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_stopPattern(
        JNIEnv *env,
        jobject /* this */,
        jobject view) {
    if (the_player != nullptr)
        the_player->song_stop();
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_vantjac_chromatracker_MainActivity_getSongPtr(
        JNIEnv *env,
        jobject /* this */) {
    if (the_player != nullptr) {
        return reinterpret_cast<jlong>(the_player->get_song());
    } else {
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_noteOn(
        JNIEnv *env, jobject, jint inst_num, jint pitch, jfloat velocity) {
    if (the_player != nullptr) {
        auto &insts = the_player->get_song()->instruments;
        int i = 0;
        chromatracker::Instrument *inst = nullptr;
        for (auto &check_inst : insts) {
            if (i == inst_num) {
                inst = &check_inst;
                break;
            }
            i++;
        }
        if (inst != nullptr) {
            chromatracker::NoteEventData data(inst, pitch, velocity);
            the_player->note_event(data);
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_noteGlide(
        JNIEnv *env, jobject, jint pitch) {
    if (the_player != nullptr) {
        chromatracker::NoteEventData data;
        data.pitch = pitch;
        the_player->note_event(data);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_noteVelocity(
        JNIEnv *env, jobject, jfloat velocity) {
    if (the_player != nullptr) {
        chromatracker::NoteEventData data;
        data.velocity = velocity;
        the_player->note_event(data);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_noteOff(
        JNIEnv *env, jobject) {
    if (the_player != nullptr) {
        chromatracker::NoteEventData data;
        data.instrument = chromatracker::EVENT_NOTE_OFF;
        the_player->note_event(data);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_vantjac_chromatracker_MainActivity_noteCut(
        JNIEnv *env, jobject) {
    if (the_player != nullptr) {
        chromatracker::NoteEventData data;
        data.instrument = chromatracker::EVENT_NOTE_OFF;
        data.velocity = 0;
        the_player->note_event(data);
    }
}
