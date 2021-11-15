#include "eventsedit.h"
#include <app.h>
#include <edit/songops.h>
#include <limits>
#include <utf8.h>

namespace chromatracker::ui::panels {

const float CELL_HEIGHT = 32;
const float TRACK_SPACING = 70;
const float TRACK_WIDTH = 64;

const glm::vec4 C_EVENT_HEAD    {1, 1, 1, 0.85};
const glm::vec4 C_GRID_CELL     {1, 1, 1, 0.4};
const glm::vec4 C_GRID_BEAT     {1, 1, 1, 0.7};
const glm::vec4 C_GRID_BAR      {0.7, 0.7, 1, 0.7};
const glm::vec4 C_EDIT_CUR      {0.5, 1, 0.5, 1};
const glm::vec4 C_EDIT_CELL     {1, 1, 1, 0.5};
const glm::vec4 C_PLAY_CUR      {0.5, 0.5, 1, 1};

EventsEdit::EventsEdit(App *app)
    : app(app)
{}

void EventsEdit::draw(Rect rect)
{
    drawTracks({rect(TL), rect(TR, {0, FONT_DEFAULT.lineHeight})});
    drawEvents({rect(TL, {0, FONT_DEFAULT.lineHeight}), rect(BR)});
}

void EventsEdit::drawTracks(Rect rect)
{
    app->scissorRect(rect);

    std::shared_lock songLock(app->song.mu);
    auto &tracks = app->song.tracks;
    if (trackEdits.size() != tracks.size())
        trackEdits.resize(tracks.size());

    for (int i = 0; i < tracks.size(); i++) {
        Rect trackR = Rect::from(TL, rect(TL, {TRACK_SPACING * i, 0}),
                                 {TRACK_WIDTH, rect.dim().y});
        trackEdits[i].draw(app, trackR, tracks[i]);
        drawText(std::to_string(i + 1), trackR(TL, {20, 0}), C_WHITE);
    }
}

void EventsEdit::drawEvents(Rect rect)
{
    app->scissorRect(rect);

    if (!editCur.cursor.section.lock()) {
        resetCursor(false);
    }

    Cursor playCur;
    {
        std::unique_lock playerLock(app->player.mu);
        playCur = app->player.cursor();
        if (followPlayback && playCur.section.lock()) {
            if (movedEditCur) {
                movedEditCur = false;
                playCur = editCur.cursor;
                app->player.setCursor(playCur);
            } else {
                editCur.cursor = playCur;
            }
        }
    }

    float timeScale = CELL_HEIGHT / cellSize;
    float scrollY;

    sampleProps.clear();
    sectionProps.clear();
    {
        Song &song = app->song;
        std::shared_lock songLock(song.mu);

        for (auto &sample : song.samples) {
            std::shared_lock sampleLock(sample->mu);
            string &name = sample->name;
            string nameAbbr;
            if (!name.empty()) {
                auto nameIt = name.begin();
                try {
                    char32_t c = utf8::next(nameIt, name.end());
                    utf8::append(c, nameAbbr);
                    if (nameIt < name.end()) {
                        c = utf8::next(nameIt, name.end());
                        utf8::append(c, nameAbbr);
                    } else {
                        nameAbbr += " ";
                    }
                } catch (utf8::exception e) {
                    nameAbbr = "??";
                }
            } else {
                nameAbbr = "  ";
            }
            sampleProps[sample] = SampleRender{nameAbbr, sample->color};
        }

        float y = 0;
        int meter = Section::NO_METER;
        for (auto &section : song.sections) {
            std::shared_lock sectionLock(section->mu);
            if (section->meter != Section::NO_METER)
                meter = section->meter;
            sectionProps[section] = SectionRender{y, section->length, meter};
            y += section->length * timeScale + 48;
        }
        scrollY = rect.dim().y / 2; // offset from top of rect
        if (auto sectionP = editCur.cursor.section.lock()) {
            scrollY -= sectionProps[sectionP].y + editCur.cursor.time * timeScale;
        }
        if (sectionEdits.size() != song.sections.size())
            sectionEdits.resize(song.sections.size());

        if (trackMutes.size() != song.tracks.size())
            trackMutes.resize(song.tracks.size());
        for (int i = 0; i < song.tracks.size(); i++) {
            std::shared_lock trackLock(song.tracks[i]->mu);
            trackMutes[i] = song.tracks[i]->mute;
        }

        for (int i = 0; i < song.sections.size(); i++) {
            auto &section = song.sections[i];
            auto &props = sectionProps[section];

            Rect sectionR = Rect::from(TL,
                rect(TL, {0, props.y + scrollY}),
                {rect.dim().x, props.length * timeScale});
            if (sectionR.max.y < rect.min.y || sectionR.min.y >= rect.max.y)
                continue;
            Rect sectionHeaderR = Rect::from(BL, sectionR(TL),
                {sectionR.dim().x, FONT_DEFAULT.lineHeight});
            // without section lock:
            sectionEdits[i].draw(app, sectionHeaderR, section);

            {
                std::shared_lock sectionLock(section->mu);
                for (int t = 0; t < section->trackEvents.size(); t++) {
                    auto &events = section->trackEvents[t];
                    SampleRender *curSampleProps = nullptr;
                    float curVelocity = 1.0f;
                    for (int e = 0; e < events.size(); e++) {
                        const Event &event = events[e];
                        ticks nextEventTime = props.length;
                        if (e != events.size() - 1)
                            nextEventTime = events[e + 1].time;
                        float startY = event.time * timeScale;
                        float endY = (nextEventTime-event.time) * timeScale;
                        Rect eventR = Rect::from(TL,
                            sectionR(TL, {TRACK_SPACING*t, startY}),
                            {TRACK_WIDTH, endY});
                        drawEvent(eventR, event, &curSampleProps, &curVelocity,
                                  trackMutes[t]);
                    }
                }
            }

            ticks barLength = TICKS_PER_BEAT * props.meter;
            for (ticks grid = 0; grid < props.length; grid += cellSize) {
                glm::vec4 color;
                if (props.meter != Section::NO_METER
                        && cellSize < barLength && grid % barLength == 0) {
                    color = C_GRID_BAR;
                } else if (cellSize < TICKS_PER_BEAT
                        && grid % TICKS_PER_BEAT == 0) {
                    color = C_GRID_BEAT;
                } else {
                    color = C_GRID_CELL;
                }
                drawRect(Rect::hLine(sectionR(TL, {0, grid * timeScale}),
                                     sectionR.right(), 1), color);
            }
            drawRect(Rect::hLine(sectionR(BL), sectionR.right(), 1),
                     C_GRID_BAR);
        } // each section
    } // songLock

    if (editCur.cursor.section.lock()) {
        drawRect(Rect::hLine(rect(CL), rect.right(), 1), C_EDIT_CUR);

        drawRect(Rect::from(TL, rect(CL, {editCur.track * TRACK_SPACING, 0}),
                            {TRACK_WIDTH, CELL_HEIGHT}), C_EDIT_CELL);
    }

    if (auto sectionP = playCur.section.lock()) {
        float playSectionY = sectionProps[sectionP].y + scrollY;
        float playCursorY = playCur.time * timeScale + playSectionY;
        drawRect(Rect::hLine(rect(TL, {0, playCursorY}), rect.right(), 1),
                 C_PLAY_CUR);
    }
}

void EventsEdit::drawEvent(Rect rect, const Event &event,
    SampleRender **curSampleProps, float *curVelocity, bool mute)
{
    string nameAbbr = "  ";
    if (auto sampleP = event.sample.lock()) {
        *curSampleProps = &sampleProps[sampleP];
        nameAbbr = (*curSampleProps)->nameAbbr;
    }
    if (event.special == Event::Special::FadeOut) {
        *curSampleProps = nullptr; // TODO gradient
    }
    if (event.velocity != Event::NO_VELOCITY) {
        *curVelocity = event.velocity;
    }

    glm::vec3 color {0, 0, 0};
    if (*curSampleProps) {
        auto &props = *curSampleProps;
        color = mute ? glm::vec3{0.3, 0.3, 0.3} : (props->color * 0.5f);
        color *= *curVelocity; // TODO gamma correct
    }

    drawRect(rect, {color, 1});
    float headThickness = event.velocity != Event::NO_VELOCITY ? 3 : 1;
    drawRect(Rect::hLine(rect(TL), rect.right(), headThickness), C_EVENT_HEAD);

    glm::vec2 textPos = rect(TL, {2, 0});
    textPos = drawText(nameAbbr, textPos, C_WHITE)(TR);
    string specialStr = " ";
    if (event.special == Event::Special::FadeOut) {
        specialStr = "=";
    } else if (event.special == Event::Special::Slide) {
        specialStr = "/";
    }
    textPos = drawText(specialStr, textPos, C_WHITE)(TR);
    if (event.pitch != Event::NO_PITCH) {
        textPos = drawText(pitchToString(event.pitch), textPos, C_WHITE)(TR);
    }
}

void EventsEdit::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    bool shift = e.keysym.mod & KMOD_SHIFT;
    bool alt = e.keysym.mod & KMOD_ALT;

    Song &song = app->song;
    play::SongPlay &player = app->player;

    switch (e.keysym.sym) {
    /* Playback */
    case SDLK_SPACE:
        if (!e.repeat) {
            std::unique_lock playerLock(player.mu);
            std::shared_lock songLock(song.mu);
            if (player.cursor().section.lock()) {
                player.fadeAll();
                snapToGrid();
            } else if (!song.sections.empty()) {
                player.setCursor(editCur.cursor);
            }
        }
        break;
    case SDLK_ESCAPE:
        if (!e.repeat) {
            std::unique_lock playerLock(player.mu);
            player.stop();
        }
        break;
    /* Navigation */
    case SDLK_SCROLLLOCK:
        if (e.repeat) break;
        followPlayback = !followPlayback;
        if (!followPlayback)
            snapToGrid();
        break;
    case SDLK_HOME:
        if (ctrl) {
            std::shared_lock songLock(song.mu);
            if (!song.sections.empty()) {
                editCur.cursor.section = song.sections.front();
            }
        }
        editCur.cursor.time = 0;
        movedEditCur = true;
        break;
    case SDLK_PAGEDOWN:
        {
            auto next = editCur.cursor.nextSection();
            if (next) {
                editCur.cursor.section = next;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_PAGEUP:
        {
            auto prev = editCur.cursor.prevSection();
            if (prev) {
                editCur.cursor.section = prev;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_RIGHT:
        {
            std::shared_lock songLock(song.mu);
            if (ctrl) {
                editCur.track = song.tracks.size() - 1;
            } else {
                editCur.track++;
                if (editCur.track >= song.tracks.size())
                    editCur.track = song.tracks.size() - 1;
            }
        }
        break;
    case SDLK_LEFT:
        if (ctrl) {
            editCur.track = 0;
        } else {
            editCur.track--;
            if (editCur.track < 0)
                editCur.track = 0;
        }
        break;
    case SDLK_DOWN:
        if (alt && !e.repeat) {
            TrackCursor searchCur = editCur;
            searchCur.cursor.time ++; // after current event
            while (auto sectionP = searchCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = searchCur.findEvent();
                if (eventIt != searchCur.events().end()) {
                    editCur.cursor.time = eventIt->time;
                    editCur.cursor.section = sectionP;
                    movedEditCur = true;
                    app->eventKeyboard.select(*eventIt);
                    app->jamEvent(e, app->eventKeyboard.selected); // no write
                    break;
                }
                searchCur.cursor.section = searchCur.cursor.nextSection();
                searchCur.cursor.time = 0;
            }
        } else if (!alt) {
            nextCell();
        }
        break;
    case SDLK_UP:
        if (alt && !e.repeat) {
            TrackCursor searchCur = editCur;
            while (auto sectionP = searchCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = searchCur.findEvent();
                if (eventIt != searchCur.events().begin()) {
                    eventIt--;
                    editCur.cursor.time = eventIt->time;
                    editCur.cursor.section = sectionP;
                    movedEditCur = true;
                    app->eventKeyboard.select(*eventIt);
                    app->jamEvent(e, app->eventKeyboard.selected); // no write
                    break;
                }
                searchCur.cursor.section = searchCur.cursor.prevSection();
                searchCur.cursor.time = std::numeric_limits<ticks>().max();
            }
        } else if (!alt) {
            prevCell();
        }
        break;
    case SDLK_RETURN:
        if (!e.repeat) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = editCur.findEvent();
                if (eventIt != editCur.events().end()) {
                    app->eventKeyboard.select(*eventIt);
                    app->jamEvent(e, app->eventKeyboard.selected); // no write
                }
            }
        }
        break;
    case SDLK_RIGHTBRACKET:
        cellSize *= ctrl ? 3 : 2;
        snapToGrid();
        break;
    case SDLK_LEFTBRACKET:
        {
            int factor = ctrl ? 3 : 2;
            if (cellSize % factor == 0)
                cellSize /= factor;
        }
        break;
    /* Commands */
    case SDLK_m:
        if (!e.repeat) {
            shared_ptr<Track> track;
            bool solo = true;
            {
                std::shared_lock songLock(song.mu);
                if (editCur.track >= 0 && editCur.track < song.tracks.size()) {
                    track = song.tracks[editCur.track];
                }

                if (ctrl && shift) {
                    for (auto &t : song.tracks) {
                        if (t != track && !t->mute) {
                            solo = false;
                            break;
                        }
                    }
                }
            }
            if (ctrl && shift) {
                app->doOperation(edit::ops::SetTrackSolo(track, !solo));
            } else if (ctrl && track) {
                bool muted;
                {
                    std::shared_lock trackLock(track->mu);
                    muted = track->mute;
                }
                app->doOperation(edit::ops::SetTrackMute(track, !muted));
            }
        }
        break;
    case SDLK_EQUALS:
        if (ctrl) {
            editCur.track++;
            {
                std::shared_lock lock(song.mu);
                if (editCur.track > song.tracks.size()) {
                    editCur.track = song.tracks.size();
                } else if (editCur.track < 0) {
                    editCur.track = 0;
                }
            }
            shared_ptr<Track> newTrack(new Track);
            app->doOperation(edit::ops::AddTrack(editCur.track, newTrack));
        }
        break;
    case SDLK_MINUS:
        if (ctrl) {
            std::shared_ptr<Track> deleteTrack;
            {
                std::shared_lock lock(song.mu);
                if (editCur.track >= 0 && editCur.track < song.tracks.size())
                    deleteTrack = song.tracks[editCur.track];
                if (editCur.track == song.tracks.size() - 1
                        && editCur.track != 0)
                    editCur.track--;
            }
            if (deleteTrack) {
                app->doOperation(edit::ops::DeleteTrack(deleteTrack));
            }
        }
        break;
    case SDLK_INSERT:
        if (ctrl && !e.repeat) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                shared_ptr<Section> newSection(new Section);
                {
                    std::shared_lock sectionLock(sectionP->mu);
                    newSection->length = sectionP->length;
                }
                int index;
                {
                    std::shared_lock songLock(song.mu);
                    newSection->trackEvents.resize(song.tracks.size());
                    auto it = editCur.cursor.findSection();
                    index = it - song.sections.begin() + 1;
                }
                app->doOperation(edit::ops::AddSection(index, newSection));
                editCur.cursor.section = newSection;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_DELETE:
        if (ctrl) {
            auto deleteSection = editCur.cursor.section.lock();
            auto next = editCur.cursor.nextSection();
            if (!next)
                next = editCur.cursor.prevSection();
            if (next) {
                editCur.cursor.section = next;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
            app->doOperation(edit::ops::DeleteSection(deleteSection));
        } else {
            app->doOperation(edit::ops::ClearCell(editCur, alt ? 1 : cellSize));
        }
        break;
    case SDLK_SLASH:
        if (!e.repeat && ctrl && editCur.cursor.time != 0) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                app->doOperation(edit::ops::SliceSection(
                    sectionP, editCur.cursor.time));
                {
                    std::shared_lock lock(sectionP->mu);
                    editCur.cursor.section = sectionP->next;
                    editCur.cursor.time = 0;
                }
            }
        }
        break;
    }
}

void EventsEdit::keyUp(const SDL_KeyboardEvent &e)
{
    SDL_Keycode sym = e.keysym.sym;
    bool alt = e.keysym.mod & KMOD_ALT;
    if (sym == SDLK_RETURN || (alt && (sym == SDLK_UP || sym == SDLK_DOWN))) {
        Event fadeEvent;
        fadeEvent.special = Event::Special::FadeOut;
        app->jamEvent(e, fadeEvent); // these keys don't write
    }
}

void EventsEdit::mouseWheel(const SDL_MouseWheelEvent &e)
{
    if (e.y < 0) {
        for (int i = 0; i < -e.y; i++) {
            nextCell();
        }
    } else if (e.y > 0) {
        for (int i = 0; i < e.y; i++) {
            prevCell();
        }
    }
}

void EventsEdit::resetCursor(bool newSong)
{
    if (newSong) {
        editCur.cursor.song = &app->song;
        editCur.track = 0;
    }
    std::shared_lock lock(app->song.mu);
    if (!app->song.sections.empty()) {
        editCur.cursor.section = app->song.sections.front();
        editCur.cursor.time = 0;
    }
}

void EventsEdit::writeEvent(bool playing, const Event &event, Event::Mask mask,
                            bool continuous)
{
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_ALT) {
        app->doOperation(edit::ops::MergeEvent(
            editCur, event, mask), continuous);
    } else if (mod & (KMOD_CAPS | KMOD_SHIFT)) {
        app->doOperation(edit::ops::WriteCell(
            editCur, playing ? 1 : cellSize, event), continuous);
        // TODO if playing, clear events
    }
    // TODO combine into single undo operation while playing
}

void EventsEdit::snapToGrid()
{
    editCur.cursor.time /= cellSize;
    editCur.cursor.time *= cellSize;
}

void EventsEdit::nextCell()
{
    snapToGrid();
    editCur.cursor.time += cellSize;
    ticks sectionLength;
    if (auto sectionP = editCur.cursor.section.lock()) {
        std::shared_lock sectionLock(sectionP->mu);
        sectionLength = sectionP->length;
    } else {
        return;
    }
    if (editCur.cursor.time >= sectionLength) {
        auto next = editCur.cursor.nextSection();
        if (next) {
            editCur.cursor.section = next;
            editCur.cursor.time = 0;
        } else {
            editCur.cursor.time = sectionLength - 1;
            snapToGrid();
        }
    }
    movedEditCur = true;
}

void EventsEdit::prevCell()
{
    if (editCur.cursor.time % cellSize != 0) {
        snapToGrid();
    } else if (editCur.cursor.time < cellSize) {
        auto prev = editCur.cursor.prevSection();
        if (prev) {
            editCur.cursor.section = prev;
            std::shared_lock sectionLock(prev->mu);
            editCur.cursor.time = prev->length - 1;
            snapToGrid();
        }
    } else {
        editCur.cursor.time -= cellSize;
    }
    movedEditCur = true;
}



} // namespace
