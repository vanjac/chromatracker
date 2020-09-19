#include <jni.h>
#include <string>
#include <cstdint>
#include "../InstSample.h"
#include "../Instrument.h"
#include "../Modulation.h"
#include "../Pattern.h"
#include "../Song.h"

#define PROPERTY_GETTER(name, object_type, return_type, function)   \
extern "C" JNIEXPORT return_type JNICALL                            \
Java_com_vantjac_chromatracker_EditProperties_##name(               \
        JNIEnv *env, jclass, jlong ptr) {                           \
    auto obj = reinterpret_cast<object_type *>(ptr);                \
    return function;                                                \
}

#define PROPERTY_SETTER(name, object_type, value_type, function)    \
extern "C" JNIEXPORT void JNICALL                                   \
Java_com_vantjac_chromatracker_EditProperties_##name(               \
        JNIEnv *env,  jclass, jlong ptr, value_type value) {        \
    auto obj = reinterpret_cast<object_type *>(ptr);                \
    function;                                                       \
}

#define PROPERTY_GETTER_OBJECT_SEQUENCE(name, object_type, function)\
extern "C" JNIEXPORT jlongArray JNICALL                             \
Java_com_vantjac_chromatracker_EditProperties_##name(               \
        JNIEnv *env, jclass, jlong ptr) {                           \
    auto obj = reinterpret_cast<object_type *>(ptr);                \
    auto &sequence = function;                                      \
    jsize size = sequence.size();                                   \
    jlong fill[size];                                               \
    int i = 0;                                                      \
    for (auto &item : sequence) {                                   \
        fill[i++] = reinterpret_cast<jlong>(&item);                 \
    }                                                               \
    jlongArray result = env->NewLongArray(size);                    \
    if (result == nullptr)                                          \
        return nullptr;                                             \
    env->SetLongArrayRegion(result, 0, size, fill);                 \
    return result;                                                  \
}

// same as above but missing an &
#define PROPERTY_GETTER_POINTER_SEQUENCE(name, object_type, function)\
extern "C" JNIEXPORT jlongArray JNICALL                             \
Java_com_vantjac_chromatracker_EditProperties_##name(               \
        JNIEnv *env, jclass, jlong ptr) {                           \
    auto obj = reinterpret_cast<object_type *>(ptr);                \
    auto &sequence = function;                                      \
    jsize size = sequence.size();                                   \
    jlong fill[size];                                               \
    int i = 0;                                                      \
    for (auto &item : sequence) {                                   \
        fill[i++] = reinterpret_cast<jlong>(item);                 \
    }                                                               \
    jlongArray result = env->NewLongArray(size);                    \
    if (result == nullptr)                                          \
        return nullptr;                                             \
    env->SetLongArrayRegion(result, 0, size, fill);                 \
    return result;                                                  \
}

using namespace chromatracker;

static void convertJNIString(JNIEnv *env,
        jstring jni_string, std::string &cpp_string) {
    const char *utf = env->GetStringUTFChars(jni_string, nullptr);
    cpp_string = utf;
    env->ReleaseStringUTFChars(jni_string, utf);
}

/*  InstSample  */

PROPERTY_GETTER(sampleGetName, InstSample, jstring,
        env->NewStringUTF(obj->name.c_str()))
PROPERTY_SETTER(sampleSetName, InstSample, jstring,
        convertJNIString(env, value, obj->name))

PROPERTY_GETTER(sampleGetWaveChannels, InstSample, jint,
        obj->wave_channels)

PROPERTY_GETTER(sampleGetWaveFrames, InstSample, jint,
        obj->wave_frames)

PROPERTY_GETTER(sampleGetWaveFrameRate, InstSample, jint,
        obj->wave_frame_rate)

PROPERTY_GETTER(sampleGetPlaybackMode, InstSample, jint,
        static_cast<jint>(obj->playback_mode))
PROPERTY_SETTER(sampleSetPlaybackMode, InstSample, jint,
        obj->playback_mode = static_cast<PlaybackMode>(value))

PROPERTY_GETTER(sampleGetLoopType, InstSample, jint,
        static_cast<jint>(obj->loop_type))
PROPERTY_SETTER(sampleSetLoopType, InstSample, jint,
        obj->loop_type = static_cast<LoopType>(value))

PROPERTY_GETTER(sampleGetLoopStart, InstSample, jint,
        obj->loop_start)
PROPERTY_SETTER(sampleSetLoopStart, InstSample, jint,
        obj->loop_start = value)

PROPERTY_GETTER(sampleGetLoopEnd, InstSample, jint,
        obj->loop_end)
PROPERTY_SETTER(sampleSetLoopEnd, InstSample, jint,
        obj->loop_end = value)

PROPERTY_GETTER(sampleGetVolume, InstSample, jfloat,
        obj->volume)
PROPERTY_SETTER(sampleSetVolume, InstSample, jfloat,
        obj->volume = value)

PROPERTY_GETTER(sampleGetPanning, InstSample, jfloat,
        obj->panning)
PROPERTY_SETTER(sampleSetPanning, InstSample, jfloat,
        obj->panning = value)

PROPERTY_GETTER(sampleGetBaseKey, InstSample, jint,
        obj->base_key)
PROPERTY_SETTER(sampleSetBaseKey, InstSample, jint,
        obj->base_key = value)

PROPERTY_GETTER(sampleGetFinetune, InstSample, jfloat,
        obj->finetune)
PROPERTY_SETTER(sampleSetFinetune, InstSample, jfloat,
        obj->finetune = value)

PROPERTY_GETTER(sampleGetKeyStart, InstSample, jint,
        obj->key_start)
PROPERTY_SETTER(sampleSetKeyStart, InstSample, jint,
        obj->key_start = value)

PROPERTY_GETTER(sampleGetKeyEnd, InstSample, jint,
        obj->key_end)
PROPERTY_SETTER(sampleSetKeyEnd, InstSample, jint,
        obj->key_end = value)

/* Instrument */

PROPERTY_GETTER(instrumentGetID0, Instrument, jchar,
        obj->id[0])
PROPERTY_SETTER(instrumentSetID0, Instrument, jchar,
        obj->id[0] = value)
PROPERTY_GETTER(instrumentGetID1, Instrument, jchar,
        obj->id[1])
PROPERTY_SETTER(instrumentSetID1, Instrument, jchar,
        obj->id[1] = value)

PROPERTY_GETTER(instrumentGetName, Instrument, jstring,
        env->NewStringUTF(obj->name.c_str()))
PROPERTY_SETTER(instrumentSetName, Instrument, jstring,
        convertJNIString(env, value, obj->name))

PROPERTY_GETTER(instrumentGetColor, Instrument, jint,
        obj->color)
PROPERTY_SETTER(instrumentSetColor, Instrument, jint,
        obj->color = value)

PROPERTY_GETTER_OBJECT_SEQUENCE(instrumentGetSamples, Instrument,
        obj->samples)

PROPERTY_GETTER(instrumentGetSampleOverlapMode, Instrument, jint,
        static_cast<jint>(obj->sample_overlap_mode))
PROPERTY_SETTER(instrumentSetSampleOverlapMode, Instrument, jint,
        obj->sample_overlap_mode = static_cast<SampleOverlapMode>(value))

PROPERTY_GETTER(instrumentGetNewNoteAction, Instrument, jint,
        static_cast<jint>(obj->new_note_action))
PROPERTY_SETTER(instrumentSetNewNoteAction, Instrument, jint,
        obj->new_note_action = static_cast<NewNoteAction>(value))

PROPERTY_GETTER(instrumentGetRandomDelay, Instrument, jint,
        obj->random_delay)
PROPERTY_SETTER(instrumentSetRandomDelay, Instrument, jint,
        obj->random_delay = value)

PROPERTY_GETTER(instrumentGetVolume, Instrument, jfloat,
        obj->volume)
PROPERTY_SETTER(instrumentSetVolume, Instrument, jfloat,
        obj->volume = value)

PROPERTY_GETTER(instrumentGetVolumeADSR, Instrument, jlong,
        reinterpret_cast<jlong>(&obj->volume_adsr))

PROPERTY_GETTER(instrumentGetPanning, Instrument, jfloat,
        obj->panning)
PROPERTY_SETTER(instrumentSetPanning, Instrument, jfloat,
        obj->panning = value)

PROPERTY_GETTER(instrumentGetTranspose, Instrument, jint,
        obj->transpose)
PROPERTY_SETTER(instrumentSetTranspose, Instrument, jint,
        obj->transpose = value)

PROPERTY_GETTER(instrumentGetFinetune, Instrument, jfloat,
        obj->finetune)
PROPERTY_SETTER(instrumentSetFinetune, Instrument, jfloat,
        obj->finetune = value)

PROPERTY_GETTER(instrumentGetGlide, Instrument, jfloat,
        obj->glide)
PROPERTY_SETTER(instrumentSetGlide, Instrument, jfloat,
        obj->glide = value)

/* ADSR */

PROPERTY_GETTER(adsrGetAttack, ADSR, jint,
        obj->attack)
PROPERTY_SETTER(adsrSetAttack, ADSR, jint,
        obj->attack = value)

PROPERTY_GETTER(adsrGetDecay, ADSR, jint,
        obj->decay)
PROPERTY_SETTER(adsrSetDecay, ADSR, jint,
        obj->decay = value)

PROPERTY_GETTER(adsrGetSustain, ADSR, jfloat,
        obj->sustain)
PROPERTY_SETTER(adsrSetSustain, ADSR, jfloat,
        obj->sustain = value)

PROPERTY_GETTER(adsrGetRelease, ADSR, jint,
        obj->release)
PROPERTY_SETTER(adsrSetRelease, ADSR, jint,
        obj->release = value)

/* NoteEventData */

PROPERTY_GETTER(noteEventGetInstrument, NoteEventData, jlong,
        reinterpret_cast<jlong>(obj->instrument))
PROPERTY_SETTER(noteEventSetInstrument, NoteEventData, jlong,
        obj->instrument = reinterpret_cast<Instrument*>(value))

PROPERTY_GETTER(noteEventGetPitch, NoteEventData, jint,
        obj->pitch)
PROPERTY_SETTER(noteEventSetPitch, NoteEventData, jint,
        obj->pitch = value)

PROPERTY_GETTER(noteEventGetVelocity, NoteEventData, jfloat,
        obj->velocity)
PROPERTY_SETTER(noteEventSetVelocity, NoteEventData, jfloat,
        obj->velocity = value)

PROPERTY_GETTER(noteEventGetMod, NoteEventData, jfloat,
        obj->mod)
PROPERTY_SETTER(noteEventSetMod, NoteEventData, jfloat,
        obj->mod = value)

PROPERTY_GETTER(noteEventGetVelocitySlide, NoteEventData, jboolean,
        obj->velocity_slide)
PROPERTY_SETTER(noteEventSetVelocitySlide, NoteEventData, jboolean,
        obj->velocity_slide = value)

PROPERTY_GETTER(noteEventGetModSlide, NoteEventData, jboolean,
        obj->mod_slide)
PROPERTY_SETTER(noteEventSetModSlide, NoteEventData, jboolean,
        obj->mod_slide = value)

/* LabelEventData */

PROPERTY_GETTER(labelEventGetText, LabelEventData, jstring,
        env->NewStringUTF(obj->text.c_str()))
PROPERTY_SETTER(labelEventSetText, LabelEventData, jstring,
        convertJNIString(env, value, obj->text))

/* Event */

PROPERTY_GETTER(eventGetTime, Event, jint,
        obj->time)
PROPERTY_SETTER(eventSetTime, Event, jint,
        obj->time = value)

PROPERTY_GETTER(eventIsNote, Event, jboolean,
        std::holds_alternative<NoteEventData>(obj->data))
PROPERTY_GETTER(eventGetNoteData, Event, jlong,
        reinterpret_cast<jlong>(&std::get<NoteEventData>(obj->data)))

PROPERTY_GETTER(eventIsLabel, Event, jboolean,
        std::holds_alternative<LabelEventData>(obj->data))
PROPERTY_GETTER(eventGetLabelData, Event, jlong,
        reinterpret_cast<jlong>(&std::get<LabelEventData>(obj->data)))

/* Pattern */

PROPERTY_GETTER(patternGetID0, Pattern, jchar,
        obj->id[0])
PROPERTY_SETTER(patternSetID0, Pattern, jchar,
        obj->id[0] = value)
PROPERTY_GETTER(patternGetID1, Pattern, jchar,
        obj->id[1])
PROPERTY_SETTER(patternSetID1, Pattern, jchar,
        obj->id[1] = value)

PROPERTY_GETTER_OBJECT_SEQUENCE(patternGetEvents, Pattern,
        obj->events)

PROPERTY_GETTER(patternGetLength, Pattern, jint,
        obj->length)
PROPERTY_SETTER(patternSetLength, Pattern, jint,
        obj->length = value)

/* Track */

PROPERTY_GETTER(trackGetName, Track, jstring,
        env->NewStringUTF(obj->name.c_str()))
PROPERTY_SETTER(trackSetName, Track, jstring,
        convertJNIString(env, value, obj->name))

PROPERTY_GETTER_OBJECT_SEQUENCE(trackGetPatterns, Track,
        obj->patterns)

PROPERTY_GETTER(trackGetMute, Track, jboolean,
        obj->mute)
PROPERTY_SETTER(trackSetMute, Track, jboolean,
        obj->mute = value)

/* Page */

PROPERTY_GETTER(pageGetLength, Page, jint,
        obj->length)
PROPERTY_SETTER(pageSetLength, Page, jint,
        obj->length = value)

PROPERTY_GETTER_POINTER_SEQUENCE(pageGetTrackPatterns, Page,
        obj->track_patterns)

PROPERTY_GETTER(pageGetTempo, Page, jint,
        obj->tempo)
PROPERTY_SETTER(pageSetTempo, Page, jint,
        obj->tempo = value)

PROPERTY_GETTER(pageGetMeter, Page, jint,
        obj->meter)
PROPERTY_SETTER(pageSetMeter, Page, jint,
        obj->meter = value)

PROPERTY_GETTER(pageGetComment, Page, jstring,
        env->NewStringUTF(obj->comment.c_str()))
PROPERTY_SETTER(pageSetComment, Page, jstring,
        convertJNIString(env, value, obj->comment))

/* Song */

PROPERTY_GETTER(songGetMasterVolume, Song, jfloat,
        obj->master_volume)
PROPERTY_SETTER(songSetMasterVolume, Song, jfloat,
        obj->master_volume = value)

PROPERTY_GETTER_OBJECT_SEQUENCE(songGetInstruments, Song,
        obj->instruments)

PROPERTY_GETTER_OBJECT_SEQUENCE(songGetTracks, Song,
        obj->tracks)

PROPERTY_GETTER_OBJECT_SEQUENCE(songGetPages, Song,
        obj->pages)
