#include <SDL2/SDL.h>
#include <math.h>

#include "song.h"
#include "instrument.h"

#define SONG_TABLE_SIZE 128
#define NUM_SAMPLES 31
#define PATTERN_LEN 64
#define MOD_TICK_SCALE 8
#define MOD_TICKS_PER_ROW 6
#define TICKS_PER_ROW (MOD_TICKS_PER_ROW * MOD_TICK_SCALE)
#define NUM_TRACKS 4
#define EVENT_SIZE 4

// TODO: other tunings??
#define NUM_TUNINGS 12*5
static const Uint16 tuning0_table[NUM_TUNINGS] = {
    1712,1616,1525,1440,1357,1281,1209,1141,1077,1017, 961, 907, // octave 0, nonstandard
     856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, // octave 1
     428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, // octave 2
     214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, // octave 3
     107, 101,  95,  90,  85,  80,  76,  71,  67,  64,  60,  57  // octave 4
};

struct ModSampleInfo {
    char inst_id[2];
    InstSample * sample;
    Uint8 finetune;
};

static ModSampleInfo sample_info[NUM_SAMPLES + 1]; // indices start at 1
static Song * current_song;

// return wave end
static int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start);
static void read_pattern(SDL_RWops * file, Pattern * pattern, int pattern_num);
static int period_to_pitch(int period, int sample_num);
static Uint8 velocity_units(int volume);
static Uint8 panning_units(int panning);
static Uint8 panning_units_coarse(int panning);
static Uint8 slide_hex_float(float slide, int bias);
static Uint8 pitch_slide_units(int pitch_slide);
static Uint8 pitch_fine_slide_units(int fine_slide);
static Uint8 velocity_slide_units(int volume_slide);
static Uint8 velocity_fine_slide_units(int fine_slide);
static int sample_add_slice(InstSample * sample, int slice_point);

void load_mod(const char * filename, Song * song) {
    // http://coppershade.org/articles/More!/Topics/Protracker_File_Format/
    printf("Loading %s\n", filename);
    current_song = song;

    SDL_RWops * file = SDL_RWFromFile(filename, "rb");
    if (!file) {
        printf("Can't open file\n");
        return;
    }

    song->tracks = vector<Track>(NUM_TRACKS);

    // first find the number of patterns
    // by searching for the highest numbered pattern in the song table
    Uint8 song_length;
    SDL_RWseek(file, 950, RW_SEEK_SET);
    SDL_RWread(file, &song_length, 1, 1);

    song->num_pages = song_length;

    SDL_RWseek(file, 952, RW_SEEK_SET);
    Uint8 song_table[SONG_TABLE_SIZE];
    SDL_RWread(file, song_table, 1, song_length);
    int max_pattern = 0;
    for (int i = 0; i < song_length; i++) {
        int pat_num = song_table[i];
        if (pat_num > max_pattern)
            max_pattern = pat_num;
        song->page_lengths[i] = PATTERN_LEN * TICKS_PER_ROW;
        for (int t = 0; t < NUM_TRACKS; t++) {
            song->tracks[t].pages[i] = pat_num;
        }
    }

    int pattern_size = NUM_TRACKS * EVENT_SIZE * PATTERN_LEN;

    int wave_pos = pattern_size * (max_pattern + 1) + 1084;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        SDL_RWseek(file, 20 + 30 * i, RW_SEEK_SET);
        InstSample * sample = new InstSample;
        int sample_num = i + 1; // sample numbers start at 1
        ModSampleInfo * info = &sample_info[sample_num];
        info->inst_id[0] = '0' + (sample_num / 10);
        info->inst_id[1] = '0' + (sample_num % 10);
        info->sample = sample;
        wave_pos = read_sample(file, sample, info, wave_pos);
        put_instrument(song, info->inst_id, sample);
    }

    for (int i = 0; i < max_pattern + 1; i++) {
        int pattern_pos = pattern_size * i + 1084;
        for (int t = 0; t < NUM_TRACKS; t++) {
            printf("Pattern %d Track %d\n", i, t);
            SDL_RWseek(file, 1084 + pattern_size * i + NUM_TRACKS * t, RW_SEEK_SET);
            read_pattern(file, &song->tracks[t].patterns[i], i);
        }
    }

    SDL_RWclose(file);
}

int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start) {
    char sample_name[22];
    SDL_RWread(file, sample_name, sizeof(sample_name), 1);

    Uint16 word_len, word_rep_pt, word_rep_len;
    Uint8 finetune, volume;
    SDL_RWread(file, &word_len, 2, 1);
    SDL_RWread(file, &finetune, 1, 1);
    SDL_RWread(file, &volume, 1, 1);
    SDL_RWread(file, &word_rep_pt, 2, 1);
    SDL_RWread(file, &word_rep_len, 2, 1);
    sample->wave_len = SDL_SwapBE16(word_len) * 2;
    word_rep_len = SDL_SwapBE16(word_rep_len);
    if (word_rep_len > 1) {
        sample->playback_mode = SMP_LOOP;
        sample->loop_start = SDL_SwapBE16(word_rep_pt) * 2;
        sample->loop_end = sample->loop_start + word_rep_len * 2;
    }
    sample->default_velocity = velocity_units(volume);
    info->finetune = finetune;

    static Sint8 wave8[65536 * 2];
    SDL_RWseek(file, wave_start, RW_SEEK_SET);
    SDL_RWread(file, wave8, sample->wave_len, 1);

    StereoFrame * wave = new StereoFrame[sample->wave_len];
    sample->wave = wave;
    for (int i = 0; i < sample->wave_len; i++) {
        float v = wave8[i] / 128.0;
        wave[i].l = wave[i].r = v;
    }

    sample->c5_freq = 8287.1369; // TODO

    return wave_start + sample->wave_len;
}


void read_pattern(SDL_RWops * file, Pattern * pattern, int pattern_num) {
    /* from OpenMPT wiki  https://wiki.openmpt.org/Manual:_Patterns

    If there is no instrument number next to a note, the previously used sample
    or instrument is recalled using the previous volume and panning settings.
    Lone instrument numbers (without a note) will reset the instrument’s or
    sample’s properties like volume (this is often used together with the
    volume slide effect to create a gated sound).
    */

    pattern->length = PATTERN_LEN * TICKS_PER_ROW;
    pattern->events.reserve(PATTERN_LEN);

    int prev_effect = -1; // always reset at start
    Uint8 prev_glide = 0;
    Uint8 prev_vibrato = 0;
    Uint8 prev_tremolo = 0;
    Uint8 prev_offset = 0;
    int sample_num_memory = 0;
    for (int i = 0; i < PATTERN_LEN; i++) {
        Uint8 bytes[EVENT_SIZE];
        SDL_RWread(file, bytes, 1, EVENT_SIZE);
        int period = ((bytes[0] & 0x0F) << 8) | bytes[1];
        int sample_num = (bytes[0] & 0xF0) | (bytes[2] >> 4);
        int effect = bytes[2] & 0x0F;
        int value = bytes[3];

        Event event {(Uint16)(i * TICKS_PER_ROW),
            {EVENT_NOTE_CHANGE, EVENT_NOTE_CHANGE}, EFFECT_NONE, 0, EFFECT_NONE, 0};
        
        int keep_empty_event = 0;
        int sub_effect = 0;
        switch (effect) {
            int speed, depth, slice_point;
            case 0x0:
                // clear previous effect
                if (prev_effect != 0x0 && prev_effect != 0x9 && prev_effect != 0xB
                    && prev_effect != 0xC && prev_effect != 0xD && prev_effect != 0xF)
                    keep_empty_event = 1;
                break;
            case 0x1:
                event.v_effect = EFFECT_PITCH_SLIDE_UP;
                event.v_value = pitch_slide_units(value);
                break;
            case 0x2:
                event.v_effect = EFFECT_PITCH_SLIDE_DOWN;
                event.v_value = pitch_slide_units(value);
                break;
            case 0x3:
                event.v_effect = EFFECT_GLIDE;
                if (value == 0)
                    event.v_value = prev_glide; // memory
                else
                    // TODO rate is doubled
                    // better to be too fast than too slow
                    event.v_value = pitch_slide_units(value * 2);
                prev_glide = event.v_value;
                break;
            case 0x4:
                event.v_effect = EFFECT_VIBRATO;
                if (value == 0)
                    event.v_value = prev_vibrato;
                else {
                    speed = value >> 4; // TODO
                    depth = value & 0xF;
                    depth /= 2;
                    event.v_value = (speed << 4) | depth;
                    prev_vibrato = event.v_value;
                }
                break;
            // 0x5 and 0x6 are combined with A below
            case 0x7:
                event.v_effect = EFFECT_TREMOLO;
                if (value == 0)
                    event.v_value = prev_tremolo;
                else {
                    speed = value >> 4; // TODO
                    depth = value & 0xF; // TODO
                    event.v_value = (speed << 4) | depth;
                    prev_tremolo = event.v_value;
                }
                break;
            case 0x8:
                event.v_effect = EFFECT_PAN;
                event.v_value = panning_units(value);
                break;
            case 0x9:
                event.v_effect = EFFECT_SAMPLE_OFFSET;
                slice_point = value * 256;
                // search for existing slice point
                if (value == 0)
                    event.v_value = prev_offset;
                if (sample_num)
                    event.v_value = sample_add_slice(sample_info[sample_num].sample, slice_point);
                else if (sample_num_memory)
                    event.v_value = sample_add_slice(sample_info[sample_num_memory].sample, slice_point);
                prev_offset = event.v_value;
                break;
            case 0xA:
            case 0x5:
            case 0x6:
                if (value & 0xF0) {
                    event.v_effect = EFFECT_VEL_SLIDE_UP;
                    event.v_value = velocity_slide_units(value >> 4);
                } else {
                    event.v_effect = EFFECT_VEL_SLIDE_DOWN;
                    event.v_value = velocity_slide_units(value & 0x0F);
                }
                // continue previous effects...
                // TODO these will get overwritten by pitch which is probably fine?
                if (effect == 0x5) {
                    // glide is more important so put it in the pitch column
                    event.p_effect = event.v_effect;
                    event.p_value = event.v_value;
                    event.v_effect = EFFECT_GLIDE;
                    event.v_value = prev_glide;
                } else if (effect == 0x6) {
                    // vibrato is less important
                    event.p_effect = EFFECT_VIBRATO;
                    event.p_value = prev_vibrato;
                }
                break;
            // TODO Position jump
            case 0xC:
                event.v_effect = EFFECT_VELOCITY;
                event.v_value = velocity_units(value);
                break;
            case 0xD: // pattern break
                // TODO jump to row
                // find each page that uses this pattern
                for (int i = 0; i < current_song->num_pages; i++) {
                    if (current_song->tracks[0].pages[i] == pattern_num) {
                        current_song->page_lengths[i] = event.time + TICKS_PER_ROW; // after this row
                    }
                }
                break;
            case 0xE:
                sub_effect = value >> 4;
                value &= 0xF;
                switch (sub_effect) {
                    case 0x1:
                        event.v_effect = EFFECT_PITCH_SLIDE_UP;
                        event.v_value = pitch_fine_slide_units(value);
                        break;
                    case 0x2:
                        event.v_effect = EFFECT_PITCH_SLIDE_DOWN;
                        event.v_value = pitch_fine_slide_units(value);
                        break;
                    case 0x5:
                        // TODO does this affect the period value?
                        // also repeated effects should not accumulate
                        event.v_effect = EFFECT_TUNE;
                        event.v_value = value; // TODO
                        break;
                    case 0x6:
                        event.instrument[0] = event.instrument[1]
                            = EVENT_PLAYBACK;
                        event.p_effect = EFFECT_REPEAT;
                        if (value != 0) {
                            event.v_effect = EFFECT_VELOCITY;
                            event.v_value = value;
                        }
                        break;
                    case 0x8:
                        event.v_effect = EFFECT_PAN;
                        event.v_value = panning_units_coarse(value);
                        break;
                    // 0x9 is checked after writing event
                    case 0xA:
                        event.v_effect = EFFECT_VEL_SLIDE_UP;
                        event.v_value = velocity_fine_slide_units(value);
                        break;
                    case 0xB:
                        event.v_effect = EFFECT_VEL_SLIDE_DOWN;
                        event.v_value = velocity_fine_slide_units(value);
                        break;
                    // 0xC is checked after writing event
                    case 0xD: // note delay
                        event.time += value * MOD_TICK_SCALE;
                        break;
                    case 0xE:
                        event.instrument[0] = event.instrument[1]
                            = EVENT_PLAYBACK;
                        event.p_effect = EFFECT_PAUSE;
                        event.v_effect = EFFECT_VELOCITY;
                        event.v_value = value * 16 * TICKS_PER_ROW / TICKS_PER_QUARTER;
                        break;
                }
                break;
            case 0xF:
                if (value >= 0x20) {
                    event.instrument[0] = event.instrument[1]
                        = EVENT_PLAYBACK;
                    event.p_effect = EFFECT_TEMPO;
                    event.v_effect = EFFECT_VELOCITY;
                    event.v_value = value;
                } else { } // TODO set speed
                break;
        }
        prev_effect = effect;

        if (period != 0) {
            // override playback event. TODO move to a different track
            if (event.instrument[0] == EVENT_PLAYBACK)
                clear_event(&event);

            // note change if glide effect, otherwise note on
            if (event.v_effect == EFFECT_GLIDE) { }
            else if (sample_num) {
                ModSampleInfo * info = &sample_info[sample_num];
                event.instrument[0] = info->inst_id[0];
                event.instrument[1] = info->inst_id[1];
            } else {
                event.instrument[0] = event.instrument[1]
                    = EVENT_REPLAY;
            }

            event.p_effect = EFFECT_PITCH;
            event.p_value = period_to_pitch(period, sample_num);
        } else if (sample_num) {
            // override playback event. TODO move to a different track
            if (event.instrument[0] == EVENT_PLAYBACK)
                clear_event(&event);

            // reset note velocity
            if (event.v_effect != EFFECT_VELOCITY) {
                // keep existing effect
                event.p_effect = event.v_effect;
                event.p_value = event.v_value;
                event.v_effect = EFFECT_VELOCITY;
                event.v_value = sample_info[sample_num].sample->default_velocity;
            }
        }

        if (!event_is_empty(event))
            pattern->events.push_back(event);
        else if (keep_empty_event) {
            event.instrument[0] = event.instrument[1]
                = EVENT_CANCEL_EFFECTS;
            pattern->events.push_back(event);
        }

        char event_str[EVENT_STR_LEN];
        event_to_string(event, event_str);
        printf("%.2X  %s", i, event_str);

        if (effect == 0xE && sub_effect == 0x9) {
            // retrigger
            Event retrigger_event {event.time,
                {EVENT_REPLAY, EVENT_REPLAY}, EFFECT_NONE, 0, EFFECT_NONE, 0};
            if (!event_is_empty(event)) {
                // skip first retrigger
                retrigger_event.time += value * MOD_TICK_SCALE;
            }
            while (retrigger_event.time < event.time + TICKS_PER_ROW) {
                pattern->events.push_back(retrigger_event);
                event_to_string(retrigger_event, event_str);
                printf("   %s", event_str);
                retrigger_event.time += value * MOD_TICK_SCALE;
            }
        } else if (effect == 0xE && sub_effect == 0xC) {
            // note cut
            Event cut_event {(Uint16)(event.time + value * MOD_TICK_SCALE),
                {EVENT_NOTE_CUT, EVENT_NOTE_CUT}, EFFECT_NONE, 0, EFFECT_NONE, 0};
                event_to_string(cut_event, event_str);
                printf("   %s", event_str);
            pattern->events.push_back(cut_event);
        }

        printf("\n");

        if (sample_num)
            sample_num_memory = sample_num;

        // skip other tracks to next row
        SDL_RWseek(file, (NUM_TRACKS - 1) * EVENT_SIZE, RW_SEEK_CUR);
    }
    printf("\n\n");
}


int period_to_pitch(int period, int sample_num) {
    // TODO binary search + different tunings
    for (int pitch = 0; pitch < NUM_TUNINGS; pitch++) {
        if (tuning0_table[pitch] == period)
            return pitch + 3 * 12;
    }
    return MIDDLE_C;
}

Uint8 velocity_units(int volume) {
    return volume * 2;
}

Uint8 panning_units(int panning) {
    if (panning > 0x80)
        panning += 1; // round up
    return panning / 2;
}

Uint8 panning_units_coarse(int panning) {
    if (panning > 0x8)
        panning += 1;
    return panning * 0x8;
}

Uint8 slide_hex_float(float slide, int bias) {
    if (slide <= 0)
        return 0;
    void * bit_ptr = &slide;
    Uint32 float_bits = *((Uint32 *)bit_ptr);

    int exp = float_bits >> 23;
    exp += bias - 127;
    unsigned mant = float_bits >> 19;
    mant &= 0xF;

    if (float_bits & (1 << 18)) { // TODO tie to even?
        mant += 1; // round up
        if (mant == 16) {
            exp += 1; // overflow
            mant = 0;
        }
    }

    if (exp < 0)
        return 0;
    if (exp > 0xF)
        return 0xFF;

    return mant | (exp << 4);
}


Uint8 pitch_slide_units(int pitch_slide) {
    // TODO!
    return pitch_fine_slide_units(pitch_slide * (MOD_TICKS_PER_ROW - 1));
}

Uint8 pitch_fine_slide_units(int fine_slide) {
    // TODO!
    // average between octaves 2 and 3
    float semis_per_row = (float)fine_slide * 12.0 / 214.0;
    float semis_per_quarter = semis_per_row * (TICKS_PER_QUARTER / TICKS_PER_ROW);
    return slide_hex_float(semis_per_quarter, PITCH_SLIDE_BIAS);
}

Uint8 velocity_slide_units(int volume_slide) {
    // TODO!
    return velocity_fine_slide_units(volume_slide * (MOD_TICKS_PER_ROW - 1));
}

Uint8 velocity_fine_slide_units(int fine_slide) {
    // TODO!
    fine_slide = velocity_units(fine_slide);
    int units_per_quarter = fine_slide * (TICKS_PER_QUARTER / TICKS_PER_ROW);
    return slide_hex_float(units_per_quarter, VELOCITY_SLIDE_BIAS);
}

int sample_add_slice(InstSample * sample, int slice_point) {
    for (int i = 0; i < sample->num_slices; i++) {
        if (sample->slices[i] == slice_point)
            return i;
    }
    // TODO check overflow
    sample->slices[sample->num_slices] = slice_point;
    sample->num_slices++;
    return sample->num_slices - 1;
}
