#include <SDL2/SDL.h>
#include <math.h>

#include "song.h"
#include "instrument.h"

#define DEBUG_EVENTS

// standard PAL rate
#define AMIGA_C5_RATE 8287.1369
#define MOD_DEFAULT_TEMPO 125
#define MOD_DEFAULT_TICKS_PER_ROW 6

#define SONG_TABLE_SIZE 128
#define MAX_PATTERNS 128
#define NUM_SAMPLES 31
#define PATTERN_LEN 64
#define ROW_TIME  (8 * MOD_DEFAULT_TICKS_PER_ROW)
#define NUM_TRACKS 4
#define EVENT_SIZE 4


#define PITCH_OFFSET 3*12
#define NUM_MOD_PITCHES 12*5
static const Uint16 tuning0_table[NUM_MOD_PITCHES] = {
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

struct ModTrackState {
    int cur_sample_num;
    int prev_effect;
    Uint8 glide_mem;
    Uint8 vibrato_mem;
    Uint8 tremolo_mem;
    Uint8 offset_mem;
    ModTrackState() : cur_sample_num(0),
        prev_effect(-1), // always reset effect at start
        glide_mem(0), vibrato_mem(0), tremolo_mem(0), offset_mem(0) { }
};

struct ModSongState {
    int tempo, ticks_per_row;
    ModSongState() : tempo(MOD_DEFAULT_TEMPO), ticks_per_row(MOD_DEFAULT_TICKS_PER_ROW) { }
};

static ModSampleInfo sample_info[NUM_SAMPLES + 1]; // indices start at 1
static Song * current_song;

// return wave end
static int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start);
static void read_pattern_cell(SDL_RWops * file, Pattern * pattern,
    int pattern_num, ModTrackState * state, ModSongState * song_state, int time);
static int period_to_pitch(int period);
static Uint8 velocity_units(int volume);
static Uint8 panning_units(int panning);
static Uint8 panning_units_coarse(int panning);
static Uint8 slide_hex_float(float slide, int bias);
static Uint8 pitch_slide_units(int pitch_slide, int ticks_per_row);
static Uint8 pitch_fine_slide_units(int fine_slide);
static Uint8 velocity_slide_units(int volume_slide, int ticks_per_row);
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
        song->page_lengths[i] = PATTERN_LEN * ROW_TIME;
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

    // state persists between patterns
    ModSongState song_state;
    ModTrackState track_states[NUM_TRACKS];
    bool pattern_read[MAX_PATTERNS] = {false};

    // read patterns in order of first appearance in sequence list
    // to roughly preserve state between patterns
    for (int j = 0; j < song_length; j++) {
        int pat_num = song_table[j];
        if (pattern_read[pat_num])
            continue;
        pattern_read[pat_num] = true;
#ifdef DEBUG_EVENTS
        printf("Pattern %d\n", pat_num);
#endif
        for (int t = 0; t < NUM_TRACKS; t++) {
            Pattern * p = &song->tracks[t].patterns[pat_num];
            p->length = PATTERN_LEN * ROW_TIME;
            p->events.reserve(PATTERN_LEN); // estimate
        }

        SDL_RWseek(file, 1084 + pattern_size * pat_num, RW_SEEK_SET);

        int time = 0;
        for (int row = 0; row < PATTERN_LEN; row++) {
#ifdef DEBUG_EVENTS
            printf("%.2X  ", row);
#endif
            for (int t = 0; t < NUM_TRACKS; t++) {
                read_pattern_cell(file, &song->tracks[t].patterns[pat_num], pat_num,
                    &track_states[t], &song_state, time);
            }
            time += ROW_TIME;
#ifdef DEBUG_EVENTS
            printf("\n");
#endif
        }
#ifdef DEBUG_EVENTS
        printf("\n\n");
#endif
    }

    SDL_RWclose(file);
}

int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start) {
    char sample_name[22];
    SDL_RWread(file, sample_name, sizeof(sample_name), 1);

    Uint16 word_len, word_rep_pt, word_rep_len;
    Sint8 finetune;
    Uint8 volume;
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
    // convert signed nibble to signed byte
    finetune <<= 4;
    finetune >>= 4;
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

    sample->c5_freq = AMIGA_C5_RATE;
    // finetune in steps of 1/8 semitone
    sample->c5_freq *= exp2f(finetune / (12.0 * 8.0));

    return wave_start + sample->wave_len;
}


void read_pattern_cell(SDL_RWops * file, Pattern * pattern,
        int pattern_num, ModTrackState * state, ModSongState * song_state, int time) {
    /* from OpenMPT wiki  https://wiki.openmpt.org/Manual:_Patterns

    If there is no instrument number next to a note, the previously used sample
    or instrument is recalled using the previous volume and panning settings.
    Lone instrument numbers (without a note) will reset the instrument’s or
    sample’s properties like volume (this is often used together with the
    volume slide effect to create a gated sound).
    */

    Uint8 bytes[EVENT_SIZE];
    SDL_RWread(file, bytes, 1, EVENT_SIZE);
    int period = ((bytes[0] & 0x0F) << 8) | bytes[1];
    int sample_num = (bytes[0] & 0xF0) | (bytes[2] >> 4);
    int effect = bytes[2] & 0x0F;
    int value = bytes[3];

    if (sample_num)
        state->cur_sample_num = sample_num;

    Event event {(Uint16)time,
        {EVENT_NOTE_CHANGE, EVENT_NOTE_CHANGE}, EFFECT_NONE, 0, EFFECT_NONE, 0};
    
    int keep_empty_event = 0;
    int sub_effect = 0;
    switch (effect) {
        case 0x0:
            // clear previous effect
            if (state->prev_effect != 0x0 && state->prev_effect != 0x9 && state->prev_effect != 0xB
                && state->prev_effect != 0xC && state->prev_effect != 0xD && state->prev_effect != 0xF)
                keep_empty_event = 1;
            break;
        case 0x1:
            event.v_effect = EFFECT_PITCH_SLIDE_UP;
            event.v_value = pitch_slide_units(value, song_state->ticks_per_row);
            break;
        case 0x2:
            event.v_effect = EFFECT_PITCH_SLIDE_DOWN;
            event.v_value = pitch_slide_units(value, song_state->ticks_per_row);
            break;
        case 0x3:
            event.v_effect = EFFECT_GLIDE;
            if (value == 0)
                event.v_value = state->glide_mem; // memory
            else
                // TODO rate is doubled
                // better to be too fast than too slow
                event.v_value = pitch_slide_units(value * 2, song_state->ticks_per_row);
            state->glide_mem = event.v_value;
            break;
        case 0x4:
            event.v_effect = EFFECT_VIBRATO;
            if (value == 0)
                event.v_value = state->vibrato_mem;
            else {
                int speed = value >> 4; // TODO
                int depth = value & 0xF;
                depth /= 2;
                event.v_value = (speed << 4) | depth;
                state->vibrato_mem = event.v_value;
            }
            break;
        // 0x5 and 0x6 are combined with A below
        case 0x7:
            event.v_effect = EFFECT_TREMOLO;
            if (value == 0)
                event.v_value = state->tremolo_mem;
            else {
                int speed = value >> 4; // TODO
                int depth = value & 0xF; // TODO
                event.v_value = (speed << 4) | depth;
                state->tremolo_mem = event.v_value;
            }
            break;
        case 0x8:
            event.v_effect = EFFECT_PAN;
            event.v_value = panning_units(value);
            break;
        case 0x9:
        {
            event.v_effect = EFFECT_SAMPLE_OFFSET;
            int slice_point = value * 256;
            // search for existing slice point
            if (value == 0)
                event.v_value = state->offset_mem;
            else if (state->cur_sample_num)
                event.v_value = sample_add_slice(sample_info[state->cur_sample_num].sample, slice_point);
            state->offset_mem = event.v_value;
            break;
        }
        case 0xA:
        case 0x5:
        case 0x6:
            if (value & 0xF0) {
                event.v_effect = EFFECT_VEL_SLIDE_UP;
                event.v_value = velocity_slide_units(value >> 4, song_state->ticks_per_row);
            } else {
                event.v_effect = EFFECT_VEL_SLIDE_DOWN;
                event.v_value = velocity_slide_units(value & 0x0F, song_state->ticks_per_row);
            }
            // continue previous effects...
            // TODO these will get overwritten by pitch which is probably fine?
            if (effect == 0x5) {
                // glide is more important so put it in the pitch column
                event.p_effect = event.v_effect;
                event.p_value = event.v_value;
                event.v_effect = EFFECT_GLIDE;
                event.v_value = state->glide_mem;
            } else if (effect == 0x6) {
                // vibrato is less important
                event.p_effect = EFFECT_VIBRATO;
                event.p_value = state->vibrato_mem;
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
                    current_song->page_lengths[i] = event.time + ROW_TIME; // after this row
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
                    // this effect only does something if there is a pitch in the note column
                    // which solves the problem that EFFECT_TUNE accumulates over multiple uses
                    if (period != 0 && state->cur_sample_num) {
                        Sint8 signed_finetune = value << 4;
                        signed_finetune >>= 4;
                        int tune_diff = signed_finetune - sample_info[state->cur_sample_num].finetune;
                        int value = tune_diff * 8 + 0x40;
                        if (value < 0)
                            value = 0;
                        if (value > 0xFF)
                            value = 0xFF;
                        event.v_effect = EFFECT_TUNE;
                        event.v_value = value;
                    }
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
                    event.time += value * ROW_TIME / song_state->ticks_per_row;
                    break;
                case 0xE:
                    event.instrument[0] = event.instrument[1]
                        = EVENT_PLAYBACK;
                    event.p_effect = EFFECT_PAUSE;
                    event.v_effect = EFFECT_VELOCITY;
                    event.v_value = value * 16 * ROW_TIME / TICKS_PER_QUARTER;
                    break;
            }
            break;
        case 0xF:
            if (value >= 0x20)
                song_state->tempo = value;
            else
                song_state->ticks_per_row = value;
            event.instrument[0] = event.instrument[1]
                = EVENT_PLAYBACK;
            event.p_effect = EFFECT_TEMPO;
            event.v_effect = EFFECT_VELOCITY;
            event.v_value = song_state->tempo * MOD_DEFAULT_TICKS_PER_ROW / song_state->ticks_per_row;
            break;
    }
    state->prev_effect = effect;

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
        event.p_value = period_to_pitch(period);
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

#ifdef DEBUG_EVENTS
    char event_str[EVENT_STR_LEN];
    event_to_string(event, event_str);
    printf("%s   ", event_str);
#endif

    if (effect == 0xE && sub_effect == 0x9) {
        // retrigger
        // Different trackers interpret retriggers without a note differently.
        // FastTracker/MilkyTracker skips the first tick.
        // OpenMPT emulates this behavior in XM mode, and does something strange in MOD mode.
        Event retrigger_event {event.time,
            {EVENT_REPLAY, EVENT_REPLAY}, EFFECT_NONE, 0, EFFECT_NONE, 0};
        if (!event_is_empty(event)) {
            // skip first retrigger
            retrigger_event.time += value * ROW_TIME / song_state->ticks_per_row;
        }
        while (retrigger_event.time < event.time + ROW_TIME) {
            pattern->events.push_back(retrigger_event);
#ifdef DEBUG_EVENTS
            event_to_string(retrigger_event, event_str);
            printf("%s   ", event_str);
#endif
            retrigger_event.time += value * ROW_TIME / song_state->ticks_per_row;
        }
    } else if (effect == 0xE && sub_effect == 0xC) {
        // note cut
        Event cut_event {(Uint16)(event.time + value * ROW_TIME / song_state->ticks_per_row),
            {EVENT_NOTE_CUT, EVENT_NOTE_CUT}, EFFECT_NONE, 0, EFFECT_NONE, 0};
#ifdef DEBUG_EVENTS
            event_to_string(cut_event, event_str);
            printf("%s   ", event_str);
#endif
        pattern->events.push_back(cut_event);
    }
}


int period_to_pitch(int period) {
    // binary search time
    // periods are in decreasing order in the tuning table
    int min = 0, max = NUM_MOD_PITCHES - 1;
    while (min <= max) {
        int i = (min + max) / 2;
        int value = tuning0_table[i];
        if (value > period)
            min = i + 1;
        else if (value < period)
            max = i - 1;
        else
            return i + PITCH_OFFSET;
    }

    // guess from the closest value
    int min_diff = abs(tuning0_table[min] - period);
    int max_diff = abs(tuning0_table[max] - period);
    if (max_diff < min_diff)
        return max + PITCH_OFFSET;
    else
        return min + PITCH_OFFSET;
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


Uint8 pitch_slide_units(int pitch_slide, int ticks_per_row) {
    return pitch_fine_slide_units(pitch_slide * (ticks_per_row - 1));
}

Uint8 pitch_fine_slide_units(int fine_slide) {
    // TODO!
    // average between octaves 2 and 3
    float semis_per_row = (float)fine_slide * 12.0 / 214.0;
    float semis_per_quarter = semis_per_row * (TICKS_PER_QUARTER / ROW_TIME);
    return slide_hex_float(semis_per_quarter, PITCH_SLIDE_BIAS);
}

Uint8 velocity_slide_units(int volume_slide, int ticks_per_row) {
    return velocity_fine_slide_units(volume_slide * (ticks_per_row - 1));
}

Uint8 velocity_fine_slide_units(int fine_slide) {
    fine_slide = velocity_units(fine_slide);
    int units_per_quarter = fine_slide * (TICKS_PER_QUARTER / ROW_TIME);
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
