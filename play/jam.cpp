#include "jam.h"
#include <algorithm>

namespace chromatracker::play {

const int NUM_JAM_TRACKS = 8;

Jam::Jam()
{
    jamTracks.resize(NUM_JAM_TRACKS);
    jamTrackTouches.resize(NUM_JAM_TRACKS);
    std::fill(jamTrackTouches.begin(), jamTrackTouches.end(), 0);
}

void Jam::stop()
{
    for (auto &track : jamTracks) {
        track.stop();
    }
    jamTouchTracks.clear();
    std::fill(jamTrackTouches.begin(), jamTrackTouches.end(), 0);
}

void Jam::queueJamEvent(const JamEvent &jam)
{
    jamEvents[(jamEventI + numJamEvents) % jamEvents.size()] = jam;
    if (numJamEvents == jamEvents.size()) {
        jamEventI++;
    } else {
        numJamEvents++;
    }
}

void Jam::processJamEvent(const JamEvent &jam)
{
    int trackIndex;
    if (jamTouchTracks.count(jam.touchId)) {
        trackIndex = jamTouchTracks[jam.touchId];
    } else {
        auto it = std::find(jamTrackTouches.begin(), jamTrackTouches.end(), 0);
        if (it == jamTrackTouches.end())
            return; // limit number of touches at once
        trackIndex = it - jamTrackTouches.begin();
        jamTouchTracks[jam.touchId] = trackIndex;
        jamTrackTouches[trackIndex] = jam.touchId;
        //cout << "assign " <<jam.touchId<< " to track " <<trackIndex<< "\n";
    }
    jamTracks[trackIndex].processEvent(jam.event);
    if (jam.event.special == Event::Special::FadeOut) {
        jamTouchTracks.erase(jam.touchId);
        jamTrackTouches[trackIndex] = 0;
    }
}

void Jam::processTick(float *tickBuffer, frames tickFrames,
                      frames outFrameRate, float globalAmp)
{
    for (int i = 0; i < numJamEvents; i++) {
        JamEvent *jam = &jamEvents[(i + jamEventI) % jamEvents.size()];
        if (jam->event.time == 0) {
            processJamEvent(*jam);
            jamEventI++;
            numJamEvents--;
            i--;
        } else {
            jam->event.time -= 1;
        }
    }

    for (auto &track : jamTracks) {
        track.processTick(tickBuffer, tickFrames,
                          outFrameRate, globalAmp, globalAmp);
    }
}

} // namespace
