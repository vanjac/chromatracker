#include "imgui.h"
#include "playback.h"

extern Sample tick_buffer[1024];
extern int tick_buffer_len;

void gui(SongPlayback * playback) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(640, 480));
    ImGui::Begin("chromatracker", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    ImGui::PlotLines("Wave", (float *)tick_buffer, tick_buffer_len * 2, 0, NULL, -1.0, 1.0, ImVec2(200, 150));

    ImGui::Text("Page: %d", playback->current_page);
    ImGui::Text("Page tick: 0x%X", playback->current_page_tick);

    ImGui::Columns(playback->num_tracks);
    ImGui::Separator();
    for (int i = 0; i < playback->num_tracks; i++) {
        TrackPlayback * track = playback->tracks + i;
        ImGui::Text("Track %d", i);
        ImGui::Text("Pattern: %d", playback->song->tracks[i].pages[playback->current_page]);
        ImGui::Text("Pattern tick: 0x%X", track->pattern_tick);
        ImGui::Text("Event: %d", track->event_i);
        ImGui::NextColumn();
    }

    ImGui::Columns(playback->num_channels);
    ImGui::Separator();
    for (int i = 0; i < playback->num_channels; i++) {
        ChannelPlayback * channel = playback->channels + i;
        ImGui::Text("Channel %d", i);
        ImGui::Text("State: %d", channel->note_state);
        ImGui::ProgressBar(channel->volume);
        ImGui::Text("Rate 0x%X", channel->playback_rate);
        int wave_len = 0;
        float * wave = NULL;
        if (channel->instrument) {
            wave_len = channel->instrument->wave_len;
            wave = (float *)channel->instrument->wave;
        }
        ImGui::PlotLines("", wave, wave_len * 2, 0, NULL, -1.0, 1.0, ImVec2(0, 60));
        int sample_pos = channel->playback_pos >> 16;
        ImGui::SliderInt("Sample", &sample_pos, 0, wave_len, "%d");
        ImGui::Text("Control: %d", CONTROL_INDEX(channel->control_command));
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::End();
}
