#ifndef CHROMATRACKER_MODULATION_H
#define CHROMATRACKER_MODULATION_H

namespace chromatracker {

struct ADSR {
ADSR();
int attack;  // ticks
int decay;
float sustain;
int release;
};

}

#endif //CHROMATRACKER_MODULATION_H
