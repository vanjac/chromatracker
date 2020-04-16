#include <math.h>
#include "pattern.h"

float note_rate(int note) {
    // TODO slow?
    return exp2f((note - MIDDLE_C) / 12.0);
}