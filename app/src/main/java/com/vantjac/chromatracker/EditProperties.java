package com.vantjac.chromatracker;

public class EditProperties {
    /* InstSample */

    public static native String sampleGetName(long ptr);
    public static native void sampleSetName(long ptr, String value);

    public static native int sampleGetWaveChannels(long ptr);

    public static native int sampleGetWaveFrames(long ptr);

    public static native int sampleGetWaveFrameRate(long ptr);

    public static native int sampleGetPlaybackMode(long ptr);
    public static native void sampleSetPlaybackMode(long ptr, int value);

    public static native int sampleGetLoopType(long ptr);
    public static native void sampleSetLoopType(long ptr, int value);

    public static native int sampleGetLoopStart(long ptr);
    public static native void sampleSetLoopStart(long ptr, int value);

    public static native int sampleGetLoopEnd(long ptr);
    public static native void sampleSetLoopEnd(long ptr, int value);

    public static native float sampleGetVolume(long ptr);
    public static native void sampleSetVolume(long ptr, float value);

    public static native float sampleGetPanning(long ptr);
    public static native void sampleSetPanning(long ptr, float value);

    public static native int sampleGetBaseKey(long ptr);
    public static native void sampleSetBaseKey(long ptr, int value);

    public static native float sampleGetFinetune(long ptr);
    public static native void sampleSetFinetune(long ptr, float value);

    public static native int sampleGetKeyStart(long ptr);
    public static native void sampleSetKeyStart(long ptr, int value);

    public static native int sampleGetKeyEnd(long ptr);
    public static native void sampleSetKeyEnd(long ptr, int value);

    /* Instrument */

    public static native char instrumentGetID0(long ptr);
    public static native void instrumentSetID0(long ptr, char value);
    public static native char instrumentGetID1(long ptr);
    public static native void instrumentSetID1(long ptr, char value);

    public static native String instrumentGetName(long ptr);
    public static native void instrumentSetName(long ptr, String value);

    public static native int instrumentGetColor(long ptr);
    public static native void instrumentSetColor(long ptr, int value);

    public static native long[] instrumentGetSamples(long ptr);

    public static native int instrumentGetSampleOverlapMode(long ptr);
    public static native void instrumentSetSampleOverlapMode(long ptr, int value);

    public static native int instrumentGetNewNoteAction(long ptr);
    public static native void instrumentSetNewNoteAction(long ptr, int value);

    public static native int instrumentGetRandomDelay(long ptr);
    public static native void instrumentSetRandomDelay(long ptr, int value);

    public static native float instrumentGetVolume(long ptr);
    public static native void instrumentSetVolume(long ptr, float value);

    public static native long instrumentGetVolumeADSR(long ptr);

    public static native float instrumentGetPanning(long ptr);
    public static native void instrumentSetPanning(long ptr, float value);

    public static native int instrumentGetTranspose(long ptr);
    public static native void instrumentSetTranspose(long ptr, int value);

    public static native float instrumentGetFinetune(long ptr);
    public static native void instrumentSetFinetune(long ptr, float value);

    public static native float instrumentGetGlide(long ptr);
    public static native void instrumentSetGlide(long ptr, float value);

    /* ADSR */

    public static native int adsrGetAttack(long ptr);
    public static native void adsrSetAttack(long ptr, int value);

    public static native int adsrGetDecay(long ptr);
    public static native void adsrSetDecay(long ptr, int value);

    public static native float adsrGetSustain(long ptr);
    public static native void adsrSetSustain(long ptr, float value);

    public static native int adsrGetRelease(long ptr);
    public static native void adsrSetRelease(long ptr, int value);

    /* NoteEventData */

    public static native long noteEventGetInstrument(long ptr);
    public static native void noteEventSetInstrument(long ptr, long value);

    public static native int noteEventGetPitch(long ptr);
    public static native void noteEventSetPitch(long ptr, int value);

    public static native float noteEventGetVelocity(long ptr);
    public static native void noteEventSetVelocity(long ptr, float value);

    public static native float noteEventGetMod(long ptr);
    public static native void noteEventSetMod(long ptr, float value);

    public static native boolean noteEventGetVelocitySlide(long ptr);
    public static native void noteEventSetVelocitySlide(long ptr, boolean value);

    public static native boolean noteEventGetModSlide(long ptr);
    public static native void noteEventSetModSlide(long ptr, boolean value);

    /* LabelEventData */

    public static native String labelEventGetText(long ptr);
    public static native void labelEventSetText(long ptr, String value);

    /* Event */

    public static native int eventGetTime(long ptr);
    public static native void eventSetTime(long ptr, int value);

    public static native boolean eventIsNote(long ptr);
    public static native long eventGetNoteData(long ptr);

    public static native boolean eventIsLabel(long ptr);
    public static native long eventGetLabelData(long ptr);

    /* Pattern */

    public static native char patternGetID0(long ptr);
    public static native void patternSetID0(long ptr, char value);
    public static native char patternGetID1(long ptr);
    public static native void patternSetID1(long ptr, char value);

    public static native long[] patternGetEvents(long ptr);

    public static native int patternGetLength(long ptr);
    public static native void patternSetLength(long ptr, int length);

    /* Track */

    public static native String trackGetName(long ptr);
    public static native void trackSetName(long ptr, String name);

    public static native long[] trackGetPatterns(long ptr);

    public static native boolean trackGetMute(long ptr);
    public static native void trackSetMute(long ptr, boolean name);

    /* Page */

    public static native int pageGetLength(long ptr);
    public static native void pageSetLength(long ptr, int length);

    public static native long[] pageGetTrackPatterns(long ptr);

    public static native int pageGetTempo(long ptr);
    public static native void pageSetTempo(long ptr, int length);

    public static native int pageGetMeter(long ptr);
    public static native void pageSetMeter(long ptr, int length);

    public static native String pageGetComment(long ptr);
    public static native void pageSetComment(long ptr, String name);

    /* Song */

    public static native float songGetMasterVolume(long ptr);
    public static native void songSetMasterVolume(long ptr, float value);

    public static native long[] songGetInstruments(long ptr);

    public static native long[] songGetTracks(long ptr);

    public static native long[] songGetPages(long ptr);
}
