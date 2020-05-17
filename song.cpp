#include "song.h"

static int id_to_index(char id[2]);
static int alphanum_to_index(char c);

void init_song(Song * song) {
    for (int i = 0; i < MAX_INST; i++)
        song->inst_table[i] = NULL;
    song->tracks = NULL;
    song->num_tracks = song->alloc_tracks = 0;

    song->num_pages = 0;
}

void free_song(Song * song) {
    for (int i = 0; i < MAX_INST; i++) {
        if (song->inst_table[i]) {
            free_inst_sample(song->inst_table[i]);
            song->inst_table[i] = NULL;
        }
    }

    if (song->tracks) {
        for (int i = 0; i < song->num_tracks; i++) {
            free_track(&song->tracks[i]);
        }
        delete song->tracks;
        song->tracks = NULL;
    }
}

int id_to_index(char id[2]) {
    int first = alphanum_to_index(id[0]);
    int second = alphanum_to_index(id[1]);
    if (first < 0 || second < 0)
        return -1;
    return first * 36 + second;
}

static int alphanum_to_index(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'Z')
        return (c - 'A') + 10;
    return -1;
}

InstSample * get_instrument(Song * song, char id[2]) {
    int index = id_to_index(id);
    if (index < 0)
        return NULL;
    return song->inst_table[index];
}

void put_instrument(Song * song, char id[2], InstSample * instrument) {
    int index = id_to_index(id);
    if (index >= 0)
        song->inst_table[index] = instrument;
}
