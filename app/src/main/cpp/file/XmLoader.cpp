#include "XmLoader.h"
#include <string>
#include <memory>
#include "../Units.h"
#include "../Log.h"
#include "../Pattern.h"
#include "../Instrument.h"

namespace chromatracker::file {

// https://github.com/milkytracker/MilkyTracker/blob/master/src/milkyplay/LoaderXM.cpp

static const int ORDER_TABLE_SIZE = 256;

XmLoader::XmLoader(const std::string &path, Song *song)
: XiLoader(path, nullptr), song(song) { }


bool XmLoader::load_xm() {
    if (!file.good()) {
        LOGE("Error opening file: %s", strerror(errno));
        return false;
    }
    LOGD("reading XM");

    std::string sig = read_string(17);
    if (sig != "Extended Module: ") {
        LOGE("bad header");
        return false;
    }

    std::string name = read_string(20);
    LOGD("Name:  %s", name.c_str());

    if (read_uint8() != 0x1A) {
        LOGE("no 1a");
        return false;
    }

    std::string tracker = read_string(20);
    LOGD("Tracker: %s", tracker.c_str());

    uint16_t version = read_uint16();
    if (version != 0x104) {
        LOGE("bad version");
        return false;
    }

    /* header */

    size_t header_size = read_uint32();
    header_size -= 4;
    if (header_size < 0x110)
        header_size = 0x110;
    size_t header_end = static_cast<size_t>(file.tellg()) + header_size;

    uint16_t num_pages = read_uint16();
    if (num_pages > ORDER_TABLE_SIZE)
        num_pages = ORDER_TABLE_SIZE;
    uint16_t restart = read_uint16();
    uint16_t num_tracks = read_uint16();
    uint16_t num_patterns = read_uint16();
    uint16_t num_instruments = read_uint16();
    uint16_t freq_tab = read_uint16();
    // swapped in MilkyTracker??
    uint16_t speed = read_uint16();
    uint16_t tempo = read_uint16();

    LOGD("Pages: %hu\nRestart: %hu\nTracks: %hu\nPatterns: %hu",
            num_pages, restart, num_tracks, num_patterns);
    LOGD("Instruments: %hu\nFreq: %hu\nSpeed: %hu\nTempo: %hu",
            num_instruments, freq_tab, speed, tempo);

    uint8_t pattern_order[ORDER_TABLE_SIZE];
    file.read(reinterpret_cast<char *>(pattern_order), ORDER_TABLE_SIZE);

    /* end of header */
    file.seekg(header_end, file.beg);

    /* set properties and create objects */

    song->master_volume = 1.0f;

    Instrument *instruments[num_instruments];
    for (int i = 0; i < num_instruments; i++) {
        song->instruments.emplace_back();
        instruments[i] = &song->instruments.back();
    }

    Pattern *patterns[num_patterns][num_tracks];
    for (int t = 0; t < num_tracks; t++) {
        song->tracks.emplace_back();
        Track *track = &song->tracks.back();
        char track_name[11];
        snprintf(track_name, sizeof(track_name), "Track %d", t);
        track->name = track_name;
        for (int p = 0; p < num_patterns; p++) {
            track->patterns.emplace_back();
            patterns[p][t] = &track->patterns.back();
        }
    }

    /* patterns */

    for (int pat_i = 0; pat_i < num_patterns; pat_i++) {
        read_pattern(patterns[pat_i], num_tracks, instruments, num_instruments,
                speed);
    }

    /* create pages */

    for (int i = 0; i < num_pages; i++) {
        song->pages.emplace_back();
        Page *page = &song->pages.back();
        int pat_num = pattern_order[i];
        page->length = patterns[pat_num][0]->length;
        for (int t = 0; t < num_tracks; t++) {
            page->track_patterns.push_back(patterns[pat_num][t]);
        }

        if (i == 0) {
            page->tempo = tempo;
            page->meter = 4;
            page->comment = name;
        }
    }

    /* instruments */

    for (int i = 0; i< num_instruments; i++) {
        size_t inst_header_size = read_uint32();
        size_t inst_header_end = static_cast<size_t>(file.tellg())
                + inst_header_size - 4;

        instruments[i]->name = read_string(22);
        LOGD("Instrument: %s", instruments[i]->name.c_str());
        uint8_t inst_type = read_uint8();

        if (inst_header_size <= 29) {
            LOGD("skip");
            read_uint16();  // num samples
        } else {
            read_inst_data(instruments[i], true, inst_header_end);
        }
    }

    return true;
}

void XmLoader::read_pattern(Pattern ** track_patterns, int num_tracks,
        Instrument ** instruments, int num_instruments, uint16_t &speed) {
    size_t pat_header_size = read_uint32();
    uint8_t pat_type = read_uint8();
    uint16_t pat_rows = read_uint16();
    uint16_t packed_data_size = read_uint16();

    if (packed_data_size == 0)
        return;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[packed_data_size]);
    file.read(reinterpret_cast<char *>(buffer.get()), packed_data_size);

    int time = 0;  // in ticks
    bool pattern_break = false;
    int pc = 0;
    for (int row_i = 0; row_i < pat_rows; row_i++) {
        int row_time = speed * TICK_TIME;
        for (int t = 0; t < num_tracks; t++) {
            uint8_t slot[5] = { 0 };

            if (buffer[pc] & 128u) {
                uint8_t pb = buffer[pc++];

                if (pb & 1u)
                    slot[0] = buffer[pc++];
                if (pb & 2u)
                    slot[1] = buffer[pc++];
                if (pb & 4u)
                    slot[2] = buffer[pc++];
                if (pb & 8u)
                    slot[3] = buffer[pc++];
                if (pb & 16u)
                    slot[4] = buffer[pc++];
            } else {
                memcpy(slot, buffer.get() + pc, 5);
                pc += 5;
            }

            uint8_t nibble1 = slot[4] >> 4u;
            uint8_t nibble2 = slot[4] & 0xFu;

            NoteEventData event_data;
            if (slot[0] == 97) {  // note off
                event_data.instrument = EVENT_NOTE_OFF;
            } else if (slot[0] != 0) {  // note
                event_data.pitch = slot[0] + KEY_OFFSET - 1;
            }
            if (slot[2] >= 0x10 && slot[2] <= 0x50) {  // volume
                event_data.velocity = amplitude_to_volume_control(
                        static_cast<float>(slot[2] - 0x10) / 64.0f);
            }
            // other effects handled below...
            if (slot[3] == 0xC) {  // volume
                event_data.velocity = amplitude_to_volume_control(
                        static_cast<float>(slot[4]) / 64.0f);
            }
            if (slot[1] != 0 && event_data.pitch != PITCH_NONE
                    && slot[1] <= num_instruments) {
                event_data.instrument = instruments[slot[1] - 1];
                if (event_data.velocity == VELOCITY_NONE)
                    event_data.velocity = 1.0f;  // must specify velocity
            }
            if (slot[3] == 0x3 || slot[2] >= 0xF0) {  // portamento
                event_data.instrument = EVENT_NOTE_GLIDE;
            }
            if (!event_data.is_empty()) {
                int event_time = time;
                if (slot[3] == 0xE && nibble1 == 0xD && nibble2 < speed)
                    event_time += nibble2 * TICK_TIME;  // delay;
                track_patterns[t]->events.emplace_back(event_time, event_data);
            }
            switch (slot[3]) {  // effect
                case 0x0:  // arpeggio, only supported when pitch is specified
                    if (slot[4] != 0 && event_data.pitch != PITCH_NONE) {
                        NoteEventData arp_event;  // glide
                        for (int i = 1; i < speed; i++) {
                            switch (i % 3) {
                                case 0:
                                    arp_event.pitch = event_data.pitch;
                                    break;
                                case 1:
                                    arp_event.pitch = event_data.pitch
                                            + nibble1;
                                    break;
                                case 2:
                                    arp_event.pitch = event_data.pitch
                                            + nibble2;
                                    break;
                            }
                            track_patterns[t]->events.emplace_back(
                                    time + i * TICK_TIME, arp_event);
                        }
                    }
                    break;
                case 0xD:  // pattern break
                    // end pattern after this row
                    pattern_break = true;
                    break;
                case 0xE: {
                    switch (nibble1) {
                        case 0x9:  // retrigger
                            if (nibble2 != 0) {
                                for (int i = nibble2; i < speed; i += nibble2) {
                                    track_patterns[t]->events.emplace_back(
                                            time + i * TICK_TIME, event_data);
                                }
                            }
                            break;
                        case 0xC:  // note cut
                            if (nibble2 != 0 && nibble2 < speed) {
                                NoteEventData cut_event;
                                cut_event.velocity = 0.0f;
                                track_patterns[t]->events.emplace_back(
                                    time + nibble2 * TICK_TIME, cut_event);
                            }
                            break;
                        case 0xE:  // pattern delay
                            row_time = (nibble2 + 1) * speed * TICK_TIME;
                            break;
                    }
                    break;
                }
                case 0xF:  // set speed (tempo not supported!)
                    if (slot[4] > 0 && slot[4] < 0x20) {
                        speed = slot[4];
                        row_time = speed * TICK_TIME;
                    }
                    break;
                case 20:  // 'K' key off
                    if (slot[4] != 0 && slot[4] < speed) {
                        NoteEventData off_event;
                        off_event.instrument = EVENT_NOTE_OFF;
                        track_patterns[t]->events.emplace_back(
                                time + slot[4] * TICK_TIME, off_event);
                    }
                    break;
                case 27:  // 'R' retrigger  (velocity change not supported)
                    if (nibble2 != 0) {
                        for (int i = nibble2; i < speed; i += nibble2) {
                            track_patterns[t]->events.emplace_back(
                                    time + i * TICK_TIME, event_data);
                        }
                    }
                    break;
            }  // end switch effect
        }  // end for each track
        time += row_time;
        if (pattern_break)  // happens *after* row
            break;
    }  // end for each row

    for (int t = 0; t < num_tracks; t++)
        track_patterns[t]->length = time;
}

}
