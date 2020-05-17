#include <stdio.h>
#include "imgui.h"
#include "playback.h"

#define EVENT_STR_LEN 11
static const char * NOTE_NAMES = "C-C#D-D#E-F-F#G-G#A-A#B-";

extern StereoFrame tick_buffer[1024];
extern int tick_buffer_len;

static void event_to_string(Event e, char * str);
static void effect_to_string(char effect, Uint8 value, char * str);

void gui(SongPlayback * playback) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(640, 480));
    ImGui::Begin("chromatracker", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImGui::PlotLines("Wave", (float *)tick_buffer, tick_buffer_len * 2, 0, NULL, -1.0, 1.0, ImVec2(200, 150));

    ImGui::Text("Page: %d", playback->current_page);
    ImGui::Text("Page tick: 0x%X", playback->current_page_tick);

    char event_str[EVENT_STR_LEN];

    ImGui::Columns(playback->num_tracks);
    ImGui::Separator();
    for (int i = 0; i < playback->num_tracks; i++) {
        TrackPlayback * track = &playback->tracks[i];
        ImGui::Text("Track %d", i);
        ImGui::Text("Pattern: %d", playback->song->tracks[i].pages[playback->current_page]);
        ImGui::Text("Pattern tick: 0x%X", track->pattern_tick);
        ImGui::Text("Event: %d", track->event_i);
        if (track->pattern) {
            Event current_event = track->pattern->events[track->event_i];
            event_to_string(current_event, event_str);
            ImGui::Text("%s", event_str);
        }
        ImGui::NextColumn();
    }

    ImGui::Columns(playback->num_channels);
    ImGui::Separator();
    for (int i = 0; i < playback->num_channels; i++) {
        ChannelPlayback * channel = &playback->channels[i];
        ImGui::Text("Channel %d", i);
        ImGui::Text("State: %d", channel->note_state);
        ImGui::ProgressBar(channel->volume);
        ImGui::Text("Pitch %f", channel->pitch_semis);
        ImGui::Text("Rate 0x%X", channel->playback_rate);
        int wave_len = 0;
        float * wave = NULL;
        if (channel->instrument) {
            wave_len = channel->instrument->wave_len;
            wave = (float *)channel->instrument->wave;
        }
        ImGui::PlotLines("", wave, wave_len * 2, 0, NULL, -1.0, 1.0, ImVec2(0, 60));
        int frame_pos = channel->playback_pos >> 16;
        ImGui::SliderInt("Sample", &frame_pos, 0, wave_len, "%d");
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::End();
}


static void event_to_string(Event e, char * str) {
    if (e.instrument[0] == EVENT_NOTE_CHANGE)
        memcpy(str, "  ", 2);
    else
        memcpy(str, e.instrument, 2);
    str[2] = ' ';
    effect_to_string(e.p_effect, e.p_value, &str[3]);
    str[6] = ' ';
    effect_to_string(e.v_effect, e.v_value, &str[7]);
    str[10] = 0;
}

static void effect_to_string(char effect, Uint8 value, char * str) {
    if (effect == EFFECT_NONE)
        memcpy(str, "   ", 3);
    else if (effect == EFFECT_PITCH) {
        int note_num = value % 12;
        memcpy(str, &NOTE_NAMES[note_num * 2], 2);
        str[2] = (value / 12) + '0';
    } else {
        if (effect == EFFECT_VELOCITY)
            str[0] = ' ';
        else
            str[0] = effect;
        sprintf(&str[1], "%.2X", value);
    }
}