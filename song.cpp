#include "song.h"

static int id_to_index(char id[2]);
static int alphanum_to_index(char c);

Song::Song()
: num_pages(0)
{
    for (int i = 0; i < MAX_INST; i++)
        inst_table[i] = NULL;
}

Song::~Song() {
    for (int i = 0; i < MAX_INST; i++)
        delete inst_table[i];
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
