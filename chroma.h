#ifndef CHROMA_H
#define CHROMA_H

#include <SDL2/SDL_stdinc.h>

typedef struct {
    float l, r;
} Sample;


typedef Uint16 ID;

#define MAX_ID ((26<<6) + 37)
#define NO_ID 0
extern const char * id_chars;

#endif
