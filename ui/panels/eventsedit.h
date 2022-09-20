#pragma once
#include <common.h>

#include "sectionedit.h"
#include "trackedit.h"
#include <cursor.h>
#include <song.h>
#include <ui/ui.h>
#include <unordered_map>
#include <SDL2/SDL_events.h>

namespace chromatracker { class App; }
namespace chromatracker::edit { class Editor; }

namespace chromatracker::ui::panels {

class EventsEdit
{
public:
    EventsEdit(App *app);

    void draw(Rect rect);
    void keyDown(const SDL_KeyboardEvent &e);
    void keyUp(const SDL_KeyboardEvent &e);
    void mouseWheel(const SDL_MouseWheelEvent &e);

private:
    // cached properties of song objects used while rendering
    struct SampleRender
    {
        string nameAbbr;
        glm::vec3 color;
    };
    struct SectionRender
    {
        float y; // starting from y = 0 at the top of the song
        ticks length;
        int meter;
    };

    void drawTracks(Rect rect);
    void drawEvents(Rect rect);
    void drawEvent(Rect rect, const Event &event,
        SampleRender **curSampleProps, float *curVelocity, bool mute);

    App * const app;
    edit::Editor * const editor;

    vector<panels::TrackEdit> trackEdits;
    vector<panels::SectionEdit> sectionEdits;

    // main loop flags
    bool movedEditCur {false}; // TODO replace with accumulator to move play cur

    // reused for each frame
    std::unordered_map<shared_ptr<const Sample>, SampleRender> sampleProps;
    std::unordered_map<shared_ptr<const Section>, SectionRender> sectionProps;
    std::vector<bool> trackMutes;
};

} // namespace
