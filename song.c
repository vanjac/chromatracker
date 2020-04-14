#include "song.h"

void init_song(Song * song) {
    for (ID id = 0; id < MAX_ID; id++)
        song->id_table[id].type = ID_EMPTY;
    song->tracks = NULL;
    song->num_tracks = song->alloc_tracks = 0;

    song->num_pages = 0;
}

void free_song(Song * song) {
    for (ID id = 0; id < MAX_ID; id++) {
        delete_id(song, id);
    }

    if (song->tracks) {
        for (int i = 0; i < song->num_tracks; i++) {
            free_track(&song->tracks[i]);
        }
        free(song->tracks);
    }
}

static IDEntry * get_id_entry(Song * song, ID id) {
    if (id >= MAX_ID)
        return 0;
    return &song->id_table[id];
}

InstSample * get_instrument(Song * song, ID id) {
    IDEntry * entry = get_id_entry(song, id);
    if (entry && entry->type == ID_INSTRUMENT)
        return entry->pointer.instrument;
    else
        return NULL;
}

void put_instrument(Song * song, ID id, InstSample * instrument) {
    IDEntry * entry = get_id_entry(song, id);
    if (entry) {
        entry->type = ID_INSTRUMENT;
        entry->pointer.instrument = instrument;
    }
}

void delete_id(Song * song, ID id) {
    IDEntry * entry = get_id_entry(song, id);
    if (!entry)
        return;
    if (entry->pointer.any) {
        switch (entry->type) {
            case ID_EMPTY:
                break;
            case ID_INSTRUMENT:
                free_inst_sample(entry->pointer.instrument);
                break;
        }
        free(entry->pointer.any);
        entry->pointer.any = NULL;
    }
}