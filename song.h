#ifndef SONG_H
#define SONG_H

#include "chroma.h"
#include "instrument.h"
#include "pattern.h"

#define MAX_INST 36 * 36

struct Song {
    Track * tracks;
    int num_tracks;
    int alloc_tracks;

    int num_pages;
    int page_lengths[MAX_PAGES]; // in ticks

    InstSample * inst_table[MAX_INST];

    Song();
    ~Song();
};

InstSample * get_instrument(Song * song, char id[2]);
void put_instrument(Song * song, char id[2], InstSample * instrument);

#endif