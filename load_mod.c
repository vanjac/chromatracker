#include <SDL2/SDL.h>

#include "song.h"
#include "instrument.h"

#define SONG_TABLE_SIZE 128
#define NUM_SAMPLES 31
#define PATTERN_LEN 64
#define TICKS_PER_ROW 48
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

typedef struct {
    Uint8 default_volume;
} ModSampleInfo;

ModSampleInfo sample_info[NUM_SAMPLES + 1]; // indices start at 1

// return wave end
static int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start);
static void read_pattern(SDL_RWops * file, Pattern * pattern);

void load_mod(char * filename, Song * song) {
    // http://coppershade.org/articles/More!/Topics/Protracker_File_Format/
    init_song(song);

    SDL_RWops * file = SDL_RWFromFile(filename, "rb");
    if (!file) {
        printf("Can't open file\n");
        return;
    }

    song->num_tracks = song->alloc_tracks = NUM_TRACKS;
    song->tracks = malloc(song->alloc_tracks * sizeof(Track));
    for (int i = 0; i < NUM_TRACKS; i++)
        init_track(&song->tracks[i]);

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
        printf("%d ", pat_num);
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
        InstSample * sample = malloc(sizeof(InstSample));
        init_inst_sample(sample);
        // sample IDs start at 1
        wave_pos = read_sample(file, sample, &sample_info[i + 1], wave_pos);
        put_instrument(song, i + 1, sample);
    }

    for (int i = 0; i < max_pattern + 1; i++) {
        int pattern_pos = pattern_size * i + 1084;
        for (int t = 0; t < NUM_TRACKS; t++) {
            SDL_RWseek(file, 1084 + pattern_size * i + NUM_TRACKS * t, RW_SEEK_SET);
            read_pattern(file, &song->tracks[t].patterns[i]);
        }
    }

    SDL_RWclose(file);
}

static int read_sample(SDL_RWops * file, InstSample * sample, ModSampleInfo * info, int wave_start) {
    init_inst_sample(sample);

    char sample_name[22];
    SDL_RWread(file, sample_name, sizeof(sample_name), 1);
    printf("%s\n", sample_name);

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
    info->default_volume = volume;

    static Sint8 wave8[65536 * 2];
    SDL_RWseek(file, wave_start, RW_SEEK_SET);
    SDL_RWread(file, wave8, sample->wave_len, 1);

    Sample * wave = malloc(sample->wave_len * sizeof(Sample));
    sample->wave = wave;
    for (int i = 0; i < sample->wave_len; i++) {
        float v = wave8[i] / 128.0;
        wave[i].l = wave[i].r = v;
    }

    sample->c5_freq = 8287.1369; // TODO

    return wave_start + sample->wave_len;
}


static void read_pattern(SDL_RWops * file, Pattern * pattern) {
    pattern->length = PATTERN_LEN * TICKS_PER_ROW;
    pattern->events = malloc(PATTERN_LEN * sizeof(Event));
    pattern->alloc_events = PATTERN_LEN;
    pattern->num_events = 0;

    int period_memory = 0;
    int sample_id_memory = 0;
    int volume_memory = 64;
    for (int i = 0; i < PATTERN_LEN; i++) {
        Uint8 bytes[EVENT_SIZE];
        SDL_RWread(file, bytes, 1, EVENT_SIZE);
        int period = ((bytes[0] & 0x0F) << 8) | bytes[1];
        ID sample_id = (bytes[0] & 0xF0) | (bytes[2] >> 4);
        int effect = bytes[2] & 0x0F;
        int params = bytes[3];

        if (period == 0 && sample_id == 0 && effect == 0) {
            // empty
        } else if (period == 0 && sample_id == 0) {
            // effect only
        } else {
            // note on
            int vol = sample_info[sample_id].default_volume;
            if (effect == 0xC) // volume
                vol = params;
            else if (period == 0) // special case for keeping previous volume
                vol = volume_memory;
            
            if (period == 0)
                period = period_memory;

            // TODO binary search + different tunings
            int note;
            for (note = 0; note < NUM_TUNINGS; note++) {
                if (tuning0_table[note] == period)
                    break;
            }
            note += 3 * 12;

            Event event = {i * TICKS_PER_ROW, sample_id, note, (Sint8)(vol * 100.0 / 64.0), 0};
            pattern->events[pattern->num_events++] = event;

            if (period != 0)
                period_memory = period;
            if (sample_id != 0)
                sample_id_memory = sample_id;
            volume_memory = vol;
        }

        // skip next 3 rows
        SDL_RWseek(file, (NUM_TRACKS - 1) * EVENT_SIZE, RW_SEEK_CUR);
    }
}