#include "app.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/theme.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <glad/glad.h>

namespace chromatracker {

using namespace ui;

const float CELL_HEIGHT = 32;
const float TRACK_SPACING = 70;
const float TRACK_WIDTH = 64;

const glm::vec4 C_GRID_CELL     {1, 1, 1, 0.4};
const glm::vec4 C_GRID_BEAT     {1, 1, 1, 0.7};
const glm::vec4 C_GRID_BAR      {0.7, 0.7, 1, 0.7};
const glm::vec4 C_EDIT_CUR      {0.5, 1, 0.5, 1};
const glm::vec4 C_EDIT_CELL     {1, 1, 1, 0.5};
const glm::vec4 C_PLAY_CUR      {0.5, 0.5, 1, 1};

void cAudioCallback(void * userdata, uint8_t *stream, int len);

App::App(SDL_Window *window)
    : window(window)
    , eventKeyboard(this)
{
    SDL_AudioSpec spec;
    spec.freq = OUT_FRAME_RATE;
    spec.format = AUDIO_F32;
    spec.channels = NUM_CHANNELS;
    spec.samples = 128;
    spec.callback = &cAudioCallback; // runs in a separate thread!
    spec.userdata = this;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!audioDevice) {
        throw std::runtime_error(string("Can't open audio device: ")
            + SDL_GetError());
    }
}

App::~App()
{
    // stop callbacks
    SDL_PauseAudioDevice(audioDevice, 1);
    SDL_CloseAudioDevice(audioDevice);
}

void App::main(const vector<string> args)
{
    // setup song (don't need locks at this moment)
    song.tracks.reserve(4);
    for (int i = 0; i < 4; i++) {
        song.tracks.emplace_back(new Track);
    }
    {
        auto section = song.sections.emplace_back(new Section);
        section->length = TICKS_PER_BEAT * 16;
        section->trackEvents.insert(section->trackEvents.end(),
            song.tracks.size(), vector<Event>());
        section->tempo = 125;
        section->meter = 4;
    }

    editCur.cursor = Cursor(&song, song.sections.front());
    player.setCursor(Cursor(&song));

    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);
    resizeWindow(winW, winH);

    SDL_PauseAudioDevice(audioDevice, 0); // audio devices start paused

    glClearColor(0, 0, 0, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);

    bool running = true;
    while (running) {
        movedEditCur = false;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    resizeWindow(event.window.data1, event.window.data2);
                }
                break;
            case SDL_KEYDOWN:
                keyDown(event.key);
                break;
            case SDL_KEYUP:
                if (!browser) {
                    eventKeyboard.keyUp(event.key);
                    keyUpEvents(event.key);
                }
                break;
            case SDL_MOUSEWHEEL:
                if (event.wheel.y < 0) {
                    for (int i = 0; i < -event.wheel.y; i++) {
                        nextCell();
                    }
                } else if (event.wheel.y > 0) {
                    for (int i = 0; i < event.wheel.y; i++) {
                        prevCell();
                    }
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                {
                    int id = event.button.button;
                    auto touch = std::make_shared<Touch>();
                    uncapturedTouches[id] = touch;
                    touch->id = id;
                    touch->button = id;
                    touch->events.push_back(event);
                    touch->pos = glm::vec2(event.button.x, event.button.y);
                }
                break;
            case SDL_MOUSEMOTION:
                for (int i = 1; i < 5; i++) {
                    if (SDL_BUTTON(i) & event.motion.state) {
                        if (auto t = findTouch(i)) {
                            t->events.push_back(event);
                            t->pos = glm::vec2(event.motion.x, event.motion.y);
                        }
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (auto t = findTouch(event.button.button)) {
                    t->events.push_back(event);
                    t->pos = glm::vec2(event.button.x, event.button.y);
                    t->captured = false;
                }
                break;
            }
        }

        if (!editCur.cursor.section.lock()) {
            // could happen after eg. undoing adding a section
            std::shared_lock lock(song.mu);
            if (!song.sections.empty()) {
                editCur.cursor.section = song.sections.front();
                editCur.cursor.time = 0;
                // don't set movedEditCur
            }
        }

        Cursor playCur;
        {
            std::unique_lock lock(player.mu);
            playCur = player.cursor();
            if (followPlayback && playCur.section.lock()) {
                if (movedEditCur) {
                    playCur = editCur.cursor;
                    player.setCursor(playCur);
                } else {
                    editCur.cursor = playCur;
                }
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);

        drawInfo({winR(TL), winR(TR, {-160, 20})});
        if (browser) {
            browser->draw({winR(TL, {0, 20}), winR(BR, {-160, -100})});
        } else {
            drawTracks({winR(TL, {0, 20}), winR(TR, {-160, 40})});
            drawEvents({winR(TL, {0, 40}), winR(BR, {-160, -100})}, playCur);
        }
        eventKeyboard.drawSampleList({winR(TR, {-160, 0}), winR(BR)});
        eventKeyboard.drawPiano({winR(BL, {0, -100}), winR(BR, {-160, 0})});

        for (auto it = uncapturedTouches.begin();
                it != uncapturedTouches.end(); ) {
            if (!it->second->captured) {
                it = uncapturedTouches.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = capturedTouches.begin(); it != capturedTouches.end(); ) {
            auto &touch = it->second;
            // widget must process touch events every frame
            if (!touch->captured || !touch->events.empty()) {
                it = capturedTouches.erase(it);
            } else {
                ++it;
            }
        }

        SDL_GL_SwapWindow(window);
    }
}

void App::resizeWindow(int w, int h)
{
    winR.max = glm::vec2(w, h);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void App::scissorRect(Rect rect) const
{
    glScissor(rect.min.x - winR.min.x, winR.max.y - rect.max.y,
              rect.dim().x, rect.dim().y);
}

void App::drawInfo(Rect rect)
{
    scissorRect(rect);

    glm::vec2 textPos = rect(TL);
    textPos = drawText(" Follow: ", textPos, C_WHITE);
    textPos = drawText(std::to_string(followPlayback), textPos, C_WHITE);
    textPos = drawText("  ", textPos, C_WHITE);

    Rect volumeR {{textPos.x, rect.top()}, rect(BR)};
    float vol;
    {
        std::shared_lock songLock(song.mu);
        vol = amplitudeToVelocity(song.volume);
    }

    if (songVolumeTouch.expired())
        songVolumeTouch = captureTouch(volumeR);
    auto touch = songVolumeTouch.lock();
    if (touch) {
        for (auto &event : touch->events) {
            if (event.type == SDL_MOUSEMOTION) {
                vol += (float)event.motion.xrel / volumeR.dim().x;
                vol = glm::clamp(vol, 0.0f, 1.0f);
                doOperation(edit::ops::SetSongVolume(velocityToAmplitude(vol)),
                            true);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                endContinuous();
            }
        }
        touch->events.clear();
    }

    drawRect(volumeR, C_DARK_GRAY * (touch ? SELECT_COLOR : NORMAL_COLOR));
    drawRect({volumeR(TL), volumeR({vol, 1})},
             C_ACCENT * (touch ? SELECT_COLOR : NORMAL_COLOR));
    drawText("Volume", textPos, C_WHITE);
}

void App::drawTracks(Rect rect)
{
    scissorRect(rect);

    std::shared_lock songLock(song.mu);
    for (int i = 0; i < song.tracks.size(); i++) {
        auto track = song.tracks[i];
        std::shared_lock trackLock(track->mu);

        Rect trackR = Rect::from(TL, rect(TL, {TRACK_SPACING * i, 0}),
                                 {TRACK_WIDTH, rect.dim().y});
        drawRect(trackR, C_DARK_GRAY);
        if (!track->mute) {
            float vol = amplitudeToVelocity(track->volume);
            drawRect({trackR(TL), trackR({vol, 0.5})}, C_ACCENT);
            drawRect({trackR(CC), trackR({track->pan / 2 + 0.5, 1})},
                     C_ACCENT);

            drawRect(Rect::vLine(trackR(CC), trackR.bottom(), 1), C_WHITE);
        }

        drawText(std::to_string(i + 1), trackR(TL, {20, 0}), C_WHITE);
    }
}

void App::drawEvents(Rect rect, Cursor playCur)
{
    scissorRect(rect);

    float timeScale = CELL_HEIGHT / cellSize;

    static std::unordered_map<shared_ptr<const Section>, float> sectionYMins;
    sectionYMins.clear();
    {
        float y = 0;
        std::shared_lock songLock(song.mu);
        for (auto &section : song.sections) {
            sectionYMins[section] = y;
            std::shared_lock sectionLock(section->mu);
            y += section->length * timeScale + 48;
        }
    }

    float scrollY = rect.dim().y / 2; // offset from top of rect
    if (auto sectionP = editCur.cursor.section.lock()) {
        scrollY -= sectionYMins[sectionP] + editCur.cursor.time * timeScale;
    }

    {
        std::shared_lock songLock(song.mu);
        int curMeter = Section::NO_METER;
        for (auto &section : song.sections) {
            std::shared_lock sectionLock(section->mu);
            if (section->meter != Section::NO_METER)
                curMeter = section->meter;
            Rect sectionR = Rect::from(TL,
                rect(TL, {0, sectionYMins[section] + scrollY}),
                {rect.dim().x, section->length * timeScale});
            if (sectionR.max.y < rect.min.y || sectionR.min.y >= rect.max.y)
                continue;

            glm::vec2 textPos = sectionR(TL, {0, -20});
            textPos = drawText(section->title, textPos, C_WHITE);
            if (section->tempo != Section::NO_TEMPO) {
                textPos = drawText("  Tempo=", textPos, C_WHITE);
                textPos = drawText(std::to_string(section->tempo), textPos,
                                   C_WHITE);
            }
            if (section->meter != Section::NO_METER) {
                textPos = drawText("  Meter=", textPos, C_WHITE);
                textPos = drawText(std::to_string(section->meter), textPos,
                                   C_WHITE);
            }
            for (int t = 0; t < section->trackEvents.size(); t++) {
                bool mute;
                {
                    auto track = song.tracks[t];
                    std::shared_lock trackLock(track->mu);
                    mute = track->mute;
                }
                auto &events = section->trackEvents[t];
                shared_ptr<Sample> curSample;
                float curVelocity = 1.0f;
                for (int e = 0; e < events.size(); e++) {
                    const Event &event = events[e];
                    auto sampleP = event.sample.lock();
                    ticks nextEventTime = section->length;
                    if (e != events.size() - 1)
                        nextEventTime = events[e + 1].time;

                    if (event.special == Event::Special::FadeOut) {
                        curSample = nullptr; // TODO gradient
                    } else if (sampleP) {
                        curSample = sampleP;
                    }
                    if (event.velocity != Event::NO_VELOCITY) {
                        curVelocity = event.velocity;
                    }

                    Rect eventR = Rect::from(TL,
                        sectionR(TL, {TRACK_SPACING*t, event.time * timeScale}),
                        {TRACK_WIDTH, (nextEventTime-event.time) * timeScale});
                    glm::vec3 color {0, 0, 0};
                    if (curSample) {
                        color = mute ? glm::vec3{0.3, 0.3, 0.3}
                            : (curSample->color * 0.5f);
                        color *= curVelocity; // TODO gamma correct
                    }
                    drawRect(eventR, {color, 1});
                    drawRect(Rect::hLine(eventR(TL), eventR.right(),
                        event.velocity != Event::NO_VELOCITY ? 3 : 1), C_WHITE);

                    // TODO avoid allocation
                    glm::vec2 textPos = eventR(TL, {2, 1});
                    if (sampleP && sampleP->name.size() >= 2) {
                        textPos = drawText(sampleP->name.substr(0, 2), textPos,
                                           C_WHITE);
                    } else if (sampleP && sampleP->name.size() == 1) {
                        textPos = drawText(sampleP->name, textPos, C_WHITE);
                        textPos = drawText(" ", textPos, C_WHITE);
                    } else {
                        textPos = drawText("  ", textPos, C_WHITE);
                    }
                    string specialStr = " ";
                    switch (event.special) {
                    case Event::Special::FadeOut:
                        specialStr = "=";
                        break;
                    case Event::Special::Slide:
                        specialStr = "/";
                        break;
                    }
                    textPos = drawText(specialStr, textPos, C_WHITE);
                    if (event.pitch != Event::NO_PITCH) {
                        textPos = drawText(pitchToString(event.pitch), textPos,
                                           C_WHITE);
                    }
                } // each event
            } // each track

            ticks barLength = TICKS_PER_BEAT * curMeter;
            for (ticks grid = 0; grid < section->length; grid += cellSize) {
                glm::vec4 color;
                if (curMeter != Section::NO_METER
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
        float playSectionY = sectionYMins[sectionP] + scrollY;
        float playCursorY = playCur.time * timeScale + playSectionY;
        drawRect(Rect::hLine(rect(TL, {0, playCursorY}), rect.right(), 1),
                 C_PLAY_CUR);
    }
}

void App::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    switch (e.keysym.sym) {
    case SDLK_z:
        if (ctrl) {
            if (!undoStack.empty()) {
                undoStack.back()->undoIt(&song);
                redoStack.push_back(std::move(undoStack.back()));
                undoStack.pop_back();
                continuousOp = nullptr;
            } else {
                cout << "Nothing to undo\n";
            }
        }
        break;
    case SDLK_y:
        if (ctrl) {
            if (!redoStack.empty()) {
                redoStack.back()->doIt(&song);
                undoStack.push_back(std::move(redoStack.back()));
                redoStack.pop_back();
                continuousOp = nullptr;
            } else {
                cout << "Nothing to redo\n";
            }
        }
        break;
    /* Mode */
    case SDLK_SCROLLLOCK:
        if (e.repeat) break;
        followPlayback = !followPlayback;
        if (!followPlayback)
            snapToGrid();
        break;
    /* Sample select */
    case SDLK_KP_PLUS:
        if (ctrl) {
            browser = std::make_unique<panels::Browser>(this,
                file::FileType::Sample, [this](file::Path path) {
                    if (path.empty()) {
                        browser.reset();
                        return;
                    }
                    unique_ptr<file::SampleLoader> loader(
                        file::sampleLoaderForPath(path));
                    if (!loader) {
                        cout << "No loader available for " <<path<< "\n";
                        browser.reset();
                        return;
                    }
                    int numSamples;
                    {
                        std::shared_lock lock(song.mu);
                        numSamples = song.samples.size();
                    }
                    shared_ptr<Sample> newSample(new Sample);
                    loader->loadSample(newSample);
                    doOperation(edit::ops::AddSample(numSamples, newSample));
                    eventKeyboard.selected.sample = newSample;

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        }
        break;
    case SDLK_KP_MINUS:
        if (ctrl) {
            if (auto sampleP = eventKeyboard.selected.sample.lock()) {
                int index;
                {
                    std::shared_lock songLock(song.mu);
                    index = eventKeyboard.sampleIndex();
                    if (index == song.samples.size() - 1)
                        index--;
                }
                doOperation(edit::ops::DeleteSample(sampleP));
                {
                    std::shared_lock songLock(song.mu);
                    if (!song.samples.empty())
                        eventKeyboard.selected.sample = song.samples[index];
                }
            }
        }
        break;
    /* File */
    case SDLK_o:
        if (ctrl) {
            browser = std::make_unique<panels::Browser>(this,
                file::FileType::Module, [this](file::Path path) {
                    if (path.empty()) {
                        browser.reset();
                        return;
                    }
                    unique_ptr<file::ModuleLoader> loader(
                        file::moduleLoaderForPath(path));
                    if (!loader) {
                        cout << "No loader available for " <<path<< "\n";
                        browser.reset();
                        return;
                    }
                    {
                        std::unique_lock playerLock(player.mu);
                        player.stop();
                    }
                    {
                        std::unique_lock lock(song.mu);
                        song.clear();
                        loader->loadSong(&song);
                        if (!song.sections.empty()) {
                            editCur.cursor.section = song.sections.front();
                            editCur.cursor.time = 0;
                        }
                    }
                    eventKeyboard.reset();

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        }
        break;
    }

    if (browser) {
        browser->keyDown(e);
    } else {
        eventKeyboard.keyDown(e);
        keyDownEvents(e);
    }
}

void App::keyDownEvents(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    bool shift = e.keysym.mod & KMOD_SHIFT;
    bool alt = e.keysym.mod & KMOD_ALT;

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
                    eventKeyboard.select(*eventIt);
                    jamEvent(e, eventKeyboard.selected); // no write
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
                    eventKeyboard.select(*eventIt);
                    jamEvent(e, eventKeyboard.selected); // no write
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
                    eventKeyboard.select(*eventIt);
                    jamEvent(e, eventKeyboard.selected); // no write
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
        if (!e.repeat && ctrl && shift) {
            bool solo = true;
            {
                std::shared_lock songLock(song.mu);
                for (int i = 0; i < song.tracks.size(); i++) {
                    if (i != editCur.track && !song.tracks[i]->mute) {
                        solo = false;
                        break;
                    }
                }
            }
            doOperation(edit::ops::SetTrackSolo(editCur.track, !solo));
        } else if (!e.repeat && ctrl) {
            shared_ptr<Track> track;
            {
                std::shared_lock songLock(song.mu);
                if (editCur.track >= 0 && editCur.track < song.tracks.size()) {
                    track = song.tracks[editCur.track];
                }
            }
            if (track) {
                bool muted;
                {
                    std::shared_lock trackLock(track->mu);
                    muted = track->mute;
                }
                doOperation(edit::ops::SetTrackMute(editCur.track, !muted));
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
            doOperation(edit::ops::AddTrack(editCur.track, newTrack));
        }
        break;
    case SDLK_MINUS:
        if (ctrl) {
            std::shared_ptr<Track> deleteTrack;
            {
                std::shared_lock lock(song.mu);
                if (editCur.track >= 0 && editCur.track < song.tracks.size())
                    deleteTrack = song.tracks[editCur.track];
            }
            if (deleteTrack) {
                doOperation(edit::ops::DeleteTrack(deleteTrack));
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
                    newSection->trackEvents.insert(
                        newSection->trackEvents.end(), song.tracks.size(),
                        vector<Event>());
                    auto it = editCur.cursor.findSection();
                    index = it - song.sections.begin() + 1;
                }
                doOperation(edit::ops::AddSection(index, newSection));
                editCur.cursor.section = newSection;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_DELETE:
        if (ctrl && shift) {
            // zap sections -- NOT undoable! (clears undo stack)
            {
                std::unique_lock playerLock(player.mu);
                player.stop();
            }
            std::unique_lock songLock(song.mu);
            for (auto section : song.sections) {
                section->deleted = true;
            }
            song.sections.clear();
            auto section = song.sections.emplace_back(new Section);
            section->length = TICKS_PER_BEAT * 16;
            section->trackEvents.insert(section->trackEvents.end(),
                song.tracks.size(), vector<Event>());
            section->tempo = 125;
            section->meter = 4;

            editCur.cursor = Cursor(&song, section);

            undoStack.clear();
            redoStack.clear();
            continuousOp = nullptr;
        } else if (ctrl) {
            auto deleteSection = editCur.cursor.section.lock();
            auto next = editCur.cursor.nextSection();
            if (!next)
                next = editCur.cursor.prevSection();
            if (next) {
                editCur.cursor.section = next;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
            doOperation(edit::ops::DeleteSection(deleteSection));
        } else {
            doOperation(edit::ops::ClearCell(editCur, alt ? 1 : cellSize));
        }
        break;
    case SDLK_SLASH:
        if (!e.repeat && ctrl && editCur.cursor.time != 0) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                doOperation(edit::ops::SliceSection(
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

void App::keyUpEvents(const SDL_KeyboardEvent &e)
{
    SDL_Keycode sym = e.keysym.sym;
    bool alt = e.keysym.mod & KMOD_ALT;
    if (sym == SDLK_RETURN || (alt && (sym == SDLK_UP || sym == SDLK_DOWN))) {
        Event fadeEvent;
        fadeEvent.special = Event::Special::FadeOut;
        jamEvent(e, fadeEvent); // these keys don't write
    }
}

void App::endContinuous()
{
    continuousOp = nullptr;
}

std::shared_ptr<ui::Touch> App::findTouch(int id)
{
    if (uncapturedTouches.count(id)) {
        return uncapturedTouches[id];
    } else if (capturedTouches.count(id)) {
        return capturedTouches[id];
    }
    return nullptr;
}

std::shared_ptr<ui::Touch> App::captureTouch(const Rect &r) {
    for (auto &pair : uncapturedTouches) {
        if (r.contains(pair.second->pos)) {
            pair.second->captured = true;
            auto touch = pair.second;
            capturedTouches[pair.first] = pair.second;
            uncapturedTouches.erase(pair.first);
            return touch;
        }
    }
    return nullptr;
}

void App::snapToGrid()
{
    editCur.cursor.time /= cellSize;
    editCur.cursor.time *= cellSize;
}

void App::nextCell()
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

void App::prevCell()
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

bool App::jamEvent(play::JamEvent jam, uint32_t timestamp)
{
    std::unique_lock playerLock(player.mu);
    jam.event.time = calcTickDelay(timestamp);
    player.jam.queueJamEvent(jam);
    return (bool)player.cursor().section.lock();
}

bool App::jamEvent(const SDL_KeyboardEvent &e, const Event &event)
{
    return jamEvent({event, -e.keysym.scancode}, e.timestamp);
}

void App::writeEvent(bool playing, const Event &event, Event::Mask mask,
                     bool continuous)
{
    SDL_Keymod mod = SDL_GetModState();
    if (mod & KMOD_ALT) {
        doOperation(edit::ops::MergeEvent(editCur, event, mask), continuous);
    } else if (mod & (KMOD_CAPS | KMOD_SHIFT)) {
        doOperation(edit::ops::WriteCell(
            editCur, playing ? 1 : cellSize, event), continuous);
        // TODO if playing, clear events while held
    }
    // TODO combine into single undo operation while playing
}

ticks App::calcTickDelay(uint32_t timestamp)
{
    if (audioCallbackTime > timestamp) {
        timestamp = 0;
    } else {
        timestamp -= audioCallbackTime;
    }
    float ticksPerSecond = player.currentTempo() * TICKS_PER_BEAT / 60.0;
    return timestamp / 1000.0 * ticksPerSecond;
}

void cAudioCallback(void * userdata, uint8_t *stream, int len)
{
    ((App *)userdata)->audioCallback(stream, len);
}

void App::audioCallback(uint8_t *stream, int len)
{
    audioCallbackTime = SDL_GetTicks();
    std::unique_lock lock(player.mu);

    float *sampleStream = (float *)stream;
    int numSamples = len / sizeof(float);
    frames numFrames = numSamples / NUM_CHANNELS;

    int writePos = 0;

    do {
        if (tickBufferPos < tickBufferLen) {
            // keep writing existing tick
            int writeLen = tickBufferLen - tickBufferPos;
            if (writeLen > numSamples - writePos) {
                writeLen = numSamples - writePos;
            }
            for (int i = 0; i < writeLen; i++) {
                sampleStream[writePos++] = tickBuffer[tickBufferPos++];
            }
        }

        if (writePos >= numSamples)
            break;

        tickBufferLen = player.processTick(tickBuffer, MAX_TICK_FRAMES,
                                           OUT_FRAME_RATE) * NUM_CHANNELS;
        tickBufferPos = 0;
    } while (tickBufferLen != 0);

    while (writePos < numSamples) {
        sampleStream[writePos++] = 0;
    }

    // clip!!
    for (int i = 0; i < numSamples; i++) {
        if (sampleStream[i] > 1.0f)
            sampleStream[i] = 1.0f;
        else if (sampleStream[i] < -1.0f)
            sampleStream[i] = -1.0f;
    }
}

} // namespace
