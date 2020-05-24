#include "imgui.h"
#include "playback.h"

extern StereoFrame tick_buffer[1024];
extern int tick_buffer_len;

void draw_pattern(Pattern * pattern, int tick);
int tick_to_ypos(int tick);

void gui(SongPlayback * playback, int width, int height) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("chromatracker", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImGui::Columns(2);

    if (playback->is_playing) {
        if (ImGui::Button("Pause")) {
            playback->is_playing = false;
            all_tracks_off(playback);
        }
    } else {
        if (ImGui::Button("Play")) {
            playback->is_playing = true;
        }
    }
    ImGui::Text("Page%4d", playback->current_page);
    ImGui::SameLine();
    if (ImGui::Button("<<")) {
        all_tracks_off(playback);
        set_playback_page(playback, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("<") && playback->current_page > 0) {
        all_tracks_off(playback);
        set_playback_page(playback, playback->current_page - 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("^")) {
        all_tracks_off(playback);
        set_playback_page(playback, playback->current_page);
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        all_tracks_off(playback);
        set_playback_page(playback, playback->current_page + 1);
    }
    ImGui::Text("Page tick:%6d", playback->current_page_tick);

    ImGui::NextColumn();
    ImGui::PlotLines("Wave", (float *)tick_buffer, tick_buffer_len * 2, 0, NULL, -1.0, 1.0, ImVec2(250, 100));
    ImGui::NextColumn();

    ImGui::Columns(playback->num_channels);
    ImGui::Separator();
    for (int i = 0; i < playback->num_channels; i++) {
        ChannelPlayback * channel = &playback->channels[i];
        ImGui::Text("Channel%4d", i);
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

    ImGui::Columns(playback->num_tracks);
    ImGui::Separator();
    for (int i = 0; i < playback->num_tracks; i++) {
        TrackPlayback * track = &playback->tracks[i];
        ImGui::Text("Track%3d", i);
        ImGui::Text("Pat %4d", playback->song->tracks[i].pages[playback->current_page]);

        char window_name[8];
        sprintf(window_name, "%d", i);
        ImGui::BeginChild(window_name, ImVec2(ImGui::GetContentRegionAvail().x, 260), false, 0);
        if (track->pattern)
            draw_pattern(track->pattern, track->pattern_tick);
        ImGui::EndChild();
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::End();
}


void draw_pattern(Pattern * pattern, int tick) {
    int margin = ImGui::GetWindowHeight() / 2;
    ImGui::SetCursorPos(ImVec2(0, tick_to_ypos(tick) + margin));
    ImGui::Separator();
    ImGui::SetScrollHereY();
    for (int i = 0; i < pattern->events.size(); i++) {
        Event e = pattern->events[i];
        char event_str[EVENT_STR_LEN];
        event_to_string(e, event_str);
        ImGui::SetCursorPos(ImVec2(0, tick_to_ypos(e.time) + margin));
        ImGui::Text("%s", event_str);
    }
    // end of pattern
    ImGui::SetCursorPos(ImVec2(0, tick_to_ypos(pattern->length) + margin));
    ImGui::Separator();
    // margin at the end
    ImGui::SetCursorPos(ImVec2(0, tick_to_ypos(pattern->length) + margin * 2));
    ImGui::Separator();
}


int tick_to_ypos(int tick) {
    return tick / 3;
}