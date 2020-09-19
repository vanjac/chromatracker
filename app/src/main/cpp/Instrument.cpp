#include "Instrument.h"
#include "Units.h"

namespace chromatracker {

Instrument::Instrument() :
id{'0', '1'},
name(""),
color(color_rgb(255, 0, 0)),
sample_overlap_mode(SampleOverlapMode::MIX),
new_note_action(NewNoteAction::OFF),
random_delay(0),
volume(1.0f),
panning(0.0f),
transpose(0),
finetune(0.0f),
glide(0.0f)
{ }

}
