#ifndef SONG_H
#define SONG_H

#include "chroma.h"
#include "instrument.h"
#include "pattern.h"

typedef struct {
    enum {ID_EMPTY=0, ID_INSTRUMENT, ID_PATTERN} type;
    union {
        void * any;
        InstSample * instrument;
    } pointer;
} IDEntry;

typedef struct {
    IDEntry id_table[MAX_ID];

    Track * tracks;
    int num_tracks;
    int alloc_tracks;

    int page_lengths[MAX_PAGES]; // in ticks
    int num_pages;
} Song;

void init_song(Song * song);
void free_song(Song * song);

InstSample * get_instrument(Song * song, ID id);
void put_instrument(Song * song, ID id, InstSample * instrument);

void delete_id(Song * song, ID id);


#endif