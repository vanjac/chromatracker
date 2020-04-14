#include "song.h"

static IDEntry * get_id_entry(ID id) {
    if (id >= MAX_ID)
        return 0;
    return &id_table[id];
}

InstSample * get_instrument(ID id) {
    IDEntry * entry = get_id_entry(id);
    if (entry && entry->type == ID_INSTRUMENT)
        return entry->pointer.instrument;
    else
        return NULL;
}

void put_instrument(ID id, InstSample * instrument) {
    IDEntry * entry = get_id_entry(id);
    if (entry) {
        entry->type = ID_INSTRUMENT;
        entry->pointer.instrument = instrument;
    }
}

void delete_id(ID id) {
    IDEntry * entry = get_id_entry(id);
    if (!entry)
        return;
    switch (entry->type) {
        case ID_EMPTY:
            break;
        case ID_INSTRUMENT:
            delete_inst_sample(entry->pointer.instrument);
            break;
    }
    entry->pointer.any = NULL;
}