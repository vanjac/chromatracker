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
        Pattern * pattern;
    } pointer;
} IDEntry;

IDEntry id_table[MAX_ID];

InstSample * get_instrument(ID id);
void put_instrument(ID id, InstSample * instrument);
Pattern * get_pattern(ID id);
void put_pattern(ID id, Pattern * pattern);

void delete_id(ID id);


Page * pages;
int num_pages;

#endif