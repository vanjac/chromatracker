#include "SongPlayback.h"
#include "../Units.h"

namespace chromatracker::play {

static int calc_tick_len(int tempo, int out_frame_rate) {
    return static_cast<int>(
        (1lu << 16lu) * static_cast<int64_t>(out_frame_rate) * 60l
        / static_cast<int64_t>(tempo) / static_cast<int64_t>(TICKS_PER_BEAT));
}

SongPlayback::SongPlayback(const Song * song, int out_frame_rate) :
        song(song),
        tick_len(calc_tick_len(DEFAULT_TEMPO, out_frame_rate)),
        tick_len_error(0),
        page_itr(song->pages.end()),
        page_time(0),
        jam_track(nullptr),
        state(out_frame_rate) { }

void SongPlayback::play_from_beginning() {
    play_from(song->pages.begin(), 0);
}

void SongPlayback::play_from(std::list<Page>::const_iterator itr, int time) {
    this->tracks.clear();
    this->tracks.reserve(song->tracks.size());
    for (const auto &track : song->tracks)
        this->tracks.emplace_back(&track);

    // search upwards for previous tempo
    int tempo = DEFAULT_TEMPO;
    while (itr != song->pages.begin()) {
        itr--;
        if (itr->tempo != TEMPO_NONE) {
            tempo = itr->tempo;
            break;
        }
    }
    this->tick_len = calc_tick_len(tempo, state.out_frame_rate);

    this->page_itr = itr;
    this->page_time = time;
    state.global_time = 0;
    update_page();
}

void SongPlayback::update_page() {
    if (this->page_itr->tempo != TEMPO_NONE) {
        this->tick_len = calc_tick_len(this->page_itr->tempo,
                state.out_frame_rate);
    }

    // TODO: what if tracks changed?
    for (int i = 0; i < this->tracks.size(); i++) {
        this->tracks[i].set_pattern(this->page_itr->track_patterns[i],
                this->page_time);
    }
}

void SongPlayback::stop_playback() {
    this->page_itr = song->pages.end();
    for (auto &track : this->tracks) {
        track.set_pattern(nullptr, 0);
        track.release_note();
    }
}

void SongPlayback::stop_all_notes() {
    for (auto &track : this->tracks) {
        track.stop_all();
    }
    jam_track.stop_all();
}

std::list<Page>::const_iterator SongPlayback::get_page() const {
    return this->page_itr;
}

int SongPlayback::get_page_time() const {
    return this->page_time;
}

void SongPlayback::jam_event(const chromatracker::Event &event) {
    jam_track.execute_event(event, &state);
}

int SongPlayback::process_tick(float *tick_buffer, int max_frames) {
    int tick_frames = static_cast<int>(this->tick_len >> 16u);
    this->tick_len_error += this->tick_len & 0xFFFFu;
    if (this->tick_len_error >= (1u << 16u)) {
        this->tick_len_error -= (1u << 16u);
        tick_frames++;
    }

    if (tick_frames > max_frames)
        tick_frames = max_frames;

    for (int i = 0; i < tick_frames * OUT_CHANNELS; i++)
        tick_buffer[i] = 0.0f;

    float amp = volume_control_to_amplitude(song->master_volume);
    for (auto &track : this->tracks)
        track.process_tick(tick_buffer, tick_frames, &this->state, amp);
    jam_track.process_tick(tick_buffer, tick_frames, &this->state, amp);

    if (this->page_itr != song->pages.end()) {
        this->page_time++;
        if (this->page_time >= this->page_itr->length) {
            this->page_time = 0;
            this->page_itr++;
            if (this->page_itr == song->pages.end())
                this->page_itr = song->pages.begin();  // loop
            update_page();
        }
    }
    state.global_time++;

    return tick_frames;
}

}