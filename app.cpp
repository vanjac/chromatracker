#include "app.h"
#include "ui/draw.h"
#include "ui/text.h"
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
const float PIANO_KEY_WIDTH = 40;
const int WHITE_KEYS[] = {0, 2, 4, 5, 7, 9, 11};
const int BLACK_KEYS[] = {-1, 1, 3, -1, 6, 8, 10};

void cAudioCallback(void * userdata, uint8_t *stream, int len);

App::App(SDL_Window *window)
    : window(window)
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

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST);

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
                if (!browser)
                    keyUpEvents(event.key);
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

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        drawInfo({winR(TL), winR(TR, {-160, 20})});
        if (browser) {
            browser->draw({winR(TL, {0, 20}), winR(BR, {-160, -100})});
        } else {
            drawTracks({winR(TL, {0, 20}), winR(TR, {-160, 40})});
            drawEvents({winR(TL, {0, 40}), winR(BR, {-160, -100})}, playCur);
        }
        drawSampleList({winR(TR, {-160, 0}), winR(BR)});
        drawPiano({winR(BL, {0, -100}), winR(BR, {-160, 0})});
        glDisable(GL_SCISSOR_TEST);

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

    glColor3f(1, 1, 1);
    glm::vec2 textPos = rect(TL);
    textPos = drawText(" Follow: ", textPos);
    textPos = drawText(std::to_string(followPlayback), textPos);
    textPos = drawText("  ", textPos);

    Rect volumeR {{textPos.x, rect.top()}, rect(BR)};
    glColor3f(0.2, 0.2, 0.2);
    drawRect(volumeR);
    float vol;
    {
        std::shared_lock songLock(song.mu);
        vol = amplitudeToVelocity(song.volume);
    }
    glColor3f(0, 0.7, 0);
    drawRect({volumeR(TL), volumeR({vol, 1})});
    glColor3f(1, 1, 1);
    drawText("Volume", textPos);
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
        glColor3f(0.2, 0.2, 0.2);
        drawRect(trackR);
        if (!track->mute) {
            float vol = amplitudeToVelocity(track->volume);
            glColor3f(0, 0.7, 0);
            drawRect({trackR(TL), trackR({vol, 0.5})});
            drawRect({trackR(CC), trackR({track->pan / 2 + 0.5, 1})});

            glColor3f(1, 1, 1);
            drawRect(Rect::vLine(trackR(CC), trackR.bottom(), 1));
        }

        glColor3f(1, 1, 1);
        drawText(std::to_string(i + 1), trackR(TL, {20, 0}));
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

            glColor3f(1, 1, 1);
            glm::vec2 textPos = sectionR(TL, {0, -20});
            textPos = drawText(section->title, textPos);
            if (section->tempo != Section::NO_TEMPO) {
                textPos = drawText("  Tempo=", textPos);
                textPos = drawText(std::to_string(section->tempo), textPos);
            }
            if (section->meter != Section::NO_METER) {
                textPos = drawText("  Meter=", textPos);
                textPos = drawText(std::to_string(section->meter), textPos);
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
                    glm::vec3 color = mute ? glm::vec3{0.3, 0.3, 0.3}
                        : glm::vec3{0.5, 0, 0.2};
                    if (!curSample)
                        color = glm::vec3(0);
                    else
                        color *= curVelocity; // TODO gamma correct
                    glColor3f(color.r, color.g, color.b);
                    drawRect(eventR);
                    glColor3f(1, 1, 1);
                    drawRect(Rect::hLine(eventR(TL), eventR.right(),
                        event.velocity != Event::NO_VELOCITY ? 3 : 1));

                    // TODO avoid allocation
                    glm::vec2 textPos = eventR(TL, {2, 1});
                    if (sampleP) {
                        textPos = drawText(sampleP->name.substr(0, 2), textPos);
                    } else {
                        textPos = drawText("  ", textPos);
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
                    textPos = drawText(specialStr, textPos);
                    if (event.pitch != Event::NO_PITCH) {
                        textPos = drawText(pitchToString(event.pitch), textPos);
                    }
                } // each event
            } // each track
            
            glEnable(GL_BLEND);
            ticks barLength = TICKS_PER_BEAT * curMeter;
            for (ticks grid = 0; grid < section->length; grid += cellSize) {
                if (curMeter != Section::NO_METER
                        && cellSize < barLength && grid % barLength == 0) {
                    glColor4f(0.7, 0.7, 1, 0.7);
                } else if (cellSize < TICKS_PER_BEAT
                        && grid % TICKS_PER_BEAT == 0) {
                    glColor4f(1, 1, 1, 0.7);
                } else {
                    glColor4f(1, 1, 1, 0.4);
                }
                drawRect(Rect::hLine(sectionR(TL, {0, grid * timeScale}),
                                     sectionR.right(), 1));
            }
            drawRect(Rect::hLine(sectionR(BL), sectionR.right(), 1));
            glDisable(GL_BLEND);
        } // each section
    } // songLock

    if (editCur.cursor.section.lock()) {
        glColor3f(0.5, 1, 0.5);
        drawRect(Rect::hLine(rect(CL), rect.right(), 1));

        glColor4f(1, 1, 1, 0.5);
        glEnable(GL_BLEND);
        drawRect(Rect::from(TL, rect(CL, {editCur.track * TRACK_SPACING, 0}),
                            {TRACK_WIDTH, CELL_HEIGHT}));
        glDisable(GL_BLEND);
    }

    if (auto sectionP = playCur.section.lock()) {
        float playSectionY = sectionYMins[sectionP] + scrollY;
        float playCursorY = playCur.time * timeScale + playSectionY;
        glColor3f(0.5, 0.5, 1);
        drawRect(Rect::hLine(rect(TL, {0, playCursorY}), rect.right(), 1));
    }
}

void App::drawSampleList(Rect rect)
{
    scissorRect(rect);
    std::shared_lock songLock(song.mu);
    auto selectedSampleP = selectedEvent.sample.lock();
    int i = 0;
    for (const auto &sample : song.samples) {
        if (sample == selectedSampleP)
            glColor3f(0.7, 1.0, 0.7);
        else
            glColor3f(1, 1, 1);
        drawText(sample->name, rect(TL, {0, (i++) * 20}));
    }
}

void App::drawPiano(Rect rect)
{
    scissorRect(rect);

    int numWhiteKeys = rect.dim().x / PIANO_KEY_WIDTH + 1;
    for (int i = 0; i < numWhiteKeys; i++) {
        int key = WHITE_KEYS[i % 7];
        int octave = selectedOctave + (i / 7);
        int pitch = key + octave * OCTAVE;

        Rect keyR = Rect::from(TL, rect(TL, {PIANO_KEY_WIDTH * i, 0}),
                               {PIANO_KEY_WIDTH, rect.dim().y});
        if (pitch == selectedEvent.pitch)
            glColor3f(0.7, 1.0, 0.7);
        else
            glColor3f(1, 1, 1);
        drawRect(keyR);

        glColor3f(0, 0, 0);
        drawRect(Rect::vLine(keyR(TL), keyR.bottom(), 1));

        if (key == 0) {
            drawText(std::to_string(octave), keyR(BL, {8, -24}));
        }
    }

    for (int i = 0; i < numWhiteKeys + 1; i++) {
        int key = BLACK_KEYS[i % 7];
        if (key < 0)
            continue;
        int pitch = key + (selectedOctave + (i / 7)) * OCTAVE;

        if (pitch == selectedEvent.pitch)
            glColor3f(0, 0.7, 0);
        else
            glColor3f(0, 0, 0);
        drawRect(Rect::from(TC, rect(TL, {i * PIANO_KEY_WIDTH, 0}),
                            {PIANO_KEY_WIDTH / 2, rect.dim().y * 0.6}));
    }

    if (selectedEvent.velocity != Event::NO_VELOCITY) {
        Rect velocityR = Rect::from(TR, rect(TR), {16, rect.dim().y});

        if (velocityTouch.expired())
            velocityTouch = captureTouch(velocityR);
        if (auto touch = velocityTouch.lock()) {
            bool left = touch->button == SDL_BUTTON_LEFT;
            bool right = touch->button == SDL_BUTTON_RIGHT;
            for (auto &event : touch->events) {
                switch (event.type) {
                case SDL_MOUSEBUTTONDOWN:
                    if (left) {
                        play::JamEvent jam {selectedEvent,
                                            (int)event.button.which + 1};
                        bool playing = jamEvent(jam, event.button.timestamp);
                        writeEvent(playing,
                                   jam.event, Event::VELOCITY, !playing);
                    } else if (right) {
                        play::JamEvent jam {selectedEvent,
                                            (int)event.button.which + 1};
                        jam.event.velocity = Event::NO_VELOCITY;
                        writeEvent(jamEvent(jam, event.button.timestamp),
                                   jam.event, Event::VELOCITY);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (left) {
                        if (selectedEvent.velocity == Event::NO_VELOCITY)
                            selectedEvent.velocity = 1;
                        selectedEvent.velocity -=
                            (float)event.motion.yrel * 2 / velocityR.dim().y;
                        if (selectedEvent.velocity > 1) {
                            selectedEvent.velocity = 1;
                        } else if (selectedEvent.velocity < 0) {
                            selectedEvent.velocity = 0;
                        }
                        play::JamEvent jam {
                            selectedEvent.masked(Event::VELOCITY),
                            (int)event.button.which + 1};
                        bool playing = jamEvent(jam, event.button.timestamp);
                        writeEvent(playing, playing ? jam.event : selectedEvent,
                                Event::VELOCITY, !playing);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (left || right) {
                        Event fadeEvent;
                        fadeEvent.special = Event::Special::FadeOut;
                        if (jamEvent({fadeEvent, (int)event.button.which + 1},
                                    event.button.timestamp)) { // if playing
                            writeEvent(true, fadeEvent, Event::ALL);
                        }
                        if (left)
                            endContinuous();
                    }
                    break;
                }
            }
            touch->events.clear();
        }

        glColor3f(0.2, 0.2, 0.2);
        drawRect(velocityR);
        glColor3f(0, 0.7, 0);
        drawRect({velocityR({0, 1 - selectedEvent.velocity}),  velocityR(BR)});
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
                    auto op = std::make_unique<edit::ops::AddSample>(
                        numSamples, newSample);
                    doOperation(std::move(op));
                    selectedEvent.sample = newSample;

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        } else {
            std::shared_lock songLock(song.mu);
            int index = selectedSampleIndex();
            if (index == -1) {
                if (!song.samples.empty())
                    selectedEvent.sample = song.samples.front();
            } else if (index < song.samples.size() - 1) {
                selectedEvent.sample = song.samples[index + 1];
            }
        }
        break;
    case SDLK_KP_MINUS:
        if (ctrl) {
            if (auto sampleP = selectedEvent.sample.lock()) {
                int index;
                {
                    std::shared_lock songLock(song.mu);
                    index = selectedSampleIndex();
                    if (index == song.samples.size() - 1)
                        index--;
                }
                auto op = std::make_unique<edit::ops::DeleteSample>(sampleP);
                doOperation(std::move(op));
                {
                    std::shared_lock songLock(song.mu);
                    if (!song.samples.empty())
                        selectedEvent.sample = song.samples[index];
                }
            }
        } else {
            std::shared_lock songLock(song.mu);
            int index = selectedSampleIndex();
            if (index > 0) {
                selectedEvent.sample = song.samples[index - 1];
            }
        }
        break;
    case SDLK_KP_MULTIPLY:
        {
            std::shared_lock songLock(song.mu);
            int index = selectedSampleIndex();
            if (index != -1 && index < song.samples.size() - 10) {
                selectedEvent.sample = song.samples[index + 10];
            } else if (!song.samples.empty()) {
                selectedEvent.sample = song.samples.back();
            }
        }
        break;
    case SDLK_KP_DIVIDE:
        {
            std::shared_lock songLock(song.mu);
            int index = selectedSampleIndex();
            if (index >= 10) {
                selectedEvent.sample = song.samples[index - 10];
            } else if (!song.samples.empty()) {
                selectedEvent.sample = song.samples.front();
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
                    std::unique_lock lock(song.mu);
                    song.clear();
                    loader->loadSong(&song);
                    if (!song.sections.empty()) {
                        editCur.cursor.section = song.sections.front();
                        editCur.cursor.time = 0;
                    }
                    if (!song.samples.empty()) {
                        selectedEvent.sample = song.samples.front();
                    }

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        }
        break;
    }

    if (browser) {
        browser->keyDown(e);
    } else {
        keyDownEvents(e);
    }
}

void App::keyDownEvents(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    bool shift = e.keysym.mod & KMOD_SHIFT;
    bool alt = e.keysym.mod & KMOD_ALT;
    if (!e.repeat && !ctrl) {
        int pitch = pitchKeymap(e.keysym.scancode);
        int sample = sampleKeymap(e.keysym.scancode);
        Event::Mask mask = Event::NO_MASK;
        if (pitch >= 0) {
            mask = Event::PITCH;
            selectedEvent.pitch = pitch + selectedOctave * OCTAVE;
        } else if (sample >= 0) {
            mask = Event::SAMPLE;
            std::shared_lock songLock(song.mu);
            int index = selectedSampleIndex();
            if (index == -1)
                index = 0;
            index = sample + (index / 10) * 10;
            if (index < song.samples.size()) {
                selectedEvent.sample = song.samples[index];
            } else {
                sample = -1;
            }
        }
        if (mask) {
            writeEvent(jamEvent(e, selectedEvent), selectedEvent, mask);
            return;
        }
    }

    switch (e.keysym.sym) {
    /* more jam */
    case SDLK_RETURN:
        if (!e.repeat) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = editCur.findEvent();
                if (eventIt != editCur.events().end()) {
                    selectEvent(*eventIt);
                    jamEvent(e, selectedEvent); // no write
                }
            }
        }
        break;
    case SDLK_BACKQUOTE:
        if (!e.repeat) {
            Event event = selectedEvent;
            event.special = Event::Special::FadeOut; // don't store
            writeEvent(jamEvent(e, event), event, Event::SPECIAL);
        }
        break;
    case SDLK_1:
        if (!e.repeat) {
            Event event = selectedEvent;
            event.special = Event::Special::Slide;
            writeEvent(jamEvent(e, event), event, Event::SPECIAL);
        }
        break;
    /* jam clear */
    case SDLK_KP_PERIOD:
        if (!e.repeat) {
            Event event = selectedEvent;
            event.sample.reset();
            writeEvent(jamEvent(e, event), event, Event::SAMPLE);
        }
        break;
    case SDLK_BACKSLASH:
        if (!e.repeat) {
            Event event = selectedEvent;
            event.pitch = Event::NO_PITCH;
            writeEvent(jamEvent(e, event), event, Event::PITCH);
        }
        break;
    case SDLK_KP_ENTER:
        if (!e.repeat) {
            // selectedEvent already shouldn't have special
            writeEvent(jamEvent(e, selectedEvent),
                       selectedEvent, Event::SPECIAL);
        }
        break;
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
                    selectEvent(*eventIt);
                    jamEvent(e, selectedEvent); // no write
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
                    selectEvent(*eventIt);
                    jamEvent(e, selectedEvent); // no write
                    break;
                }
                searchCur.cursor.section = searchCur.cursor.prevSection();
                searchCur.cursor.time = std::numeric_limits<ticks>().max();
            }
        } else if (!alt) {
            prevCell();
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
            auto op = std::make_unique<edit::ops::SetTrackSolo>(
                editCur.track, !solo);
            doOperation(std::move(op));
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
                auto op = std::make_unique<edit::ops::SetTrackMute>(
                    editCur.track, !muted);
                doOperation(std::move(op));
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
            auto op = std::make_unique<edit::ops::AddTrack>(
                editCur.track, newTrack);
            doOperation(std::move(op));
        } else if (selectedOctave < 9) {
            selectedOctave++;
            if (selectedEvent.pitch != Event::NO_PITCH)
                selectedEvent.pitch += 12;
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
                auto op = std::make_unique<edit::ops::DeleteTrack>(deleteTrack);
                doOperation(std::move(op));
            }
        } else if (selectedOctave > 0) {
            selectedOctave--;
            if (selectedEvent.pitch != Event::NO_PITCH)
                selectedEvent.pitch -= 12;
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
                auto op = std::make_unique<edit::ops::AddSection>(
                    index, newSection);
                doOperation(std::move(op));
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
            auto op = std::make_unique<edit::ops::DeleteSection>(
                deleteSection);
            doOperation(std::move(op));
        } else {
            auto op = std::make_unique<edit::ops::ClearCell>(
                editCur, alt ? 1 : cellSize);
            doOperation(std::move(op));
        }
        break;
    case SDLK_SLASH:
        if (!e.repeat && ctrl && editCur.cursor.time != 0) {
            if (auto sectionP = editCur.cursor.section.lock()) {
                auto op = std::make_unique<edit::ops::SliceSection>(
                    sectionP, editCur.cursor.time);
                doOperation(std::move(op));
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
    int pitch = -1, sampleI = -1;
    if (!(e.keysym.mod & KMOD_CTRL)) {
        pitch = pitchKeymap(e.keysym.scancode);
        sampleI = sampleKeymap(e.keysym.scancode);
    }
    bool write = true;
    if (pitch < 0 && sampleI < 0) {
        switch (e.keysym.sym) {
            // not SDLK_BACKQUOTE (already FadeOut)
            case SDLK_1:
            case SDLK_KP_PERIOD:
            case SDLK_BACKSLASH:
            case SDLK_KP_ENTER:
                break; // good
            case SDLK_RETURN:
                write = false;
                break;
            case SDLK_UP:
            case SDLK_DOWN:
                if (!(e.keysym.mod & KMOD_ALT))
                    return;
                write = false;
                break;
            default:
                return;
        }
    }

    Event fadeEvent;
    fadeEvent.special = Event::Special::FadeOut;
    if (jamEvent(e, fadeEvent) && write) { // if playing and write
        writeEvent(true, fadeEvent, Event::ALL);
    }
}

void App::doOperation(unique_ptr<edit::SongOp> op, bool continuous)
{
    if (op->doIt(&song)) {
        if (continuous) {
            continuousOp = op.get();
        } else {
            continuousOp = nullptr;
        }
        undoStack.push_back(std::move(op));
        redoStack.clear();
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

int App::selectedSampleIndex()
{
    // indices are easier to work with than iterators in this case
    auto it = std::find(song.samples.begin(), song.samples.end(),
                        selectedEvent.sample.lock());
    if (it == song.samples.end())
        return -1;
    else
        return it - song.samples.begin();
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

void App::selectEvent(const Event &event)
{
    selectedEvent.merge(event);
    selectedEvent.special = Event::Special::None; // don't store
    if (event.pitch != Event::NO_PITCH)
        selectedOctave = selectedEvent.pitch / OCTAVE;
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
        // TODO this could all be done with a template
        auto prevOp = dynamic_cast<edit::ops::MergeEvent*>(continuousOp);
        if (continuous && prevOp) {
            prevOp->undoIt(&song);
            *prevOp = edit::ops::MergeEvent(editCur, event, mask);
            prevOp->doIt(&song);
        } else {
            auto op = std::make_unique<edit::ops::MergeEvent>(
                editCur, event, mask);
            doOperation(std::move(op), continuous);
        }
    } else if (mod & (KMOD_CAPS | KMOD_SHIFT)) {
        ticks size = playing ? 1 : cellSize;

        auto prevOp = dynamic_cast<edit::ops::WriteCell*>(continuousOp);
        if (continuous && prevOp) {
            prevOp->undoIt(&song);
            *prevOp = edit::ops::WriteCell(editCur, size, event);
            prevOp->doIt(&song);
        } else {
            auto op = std::make_unique<edit::ops::WriteCell>(
                editCur, !playing ? cellSize : 1, event);
            doOperation(std::move(op), continuous);
        }
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

int App::pitchKeymap(SDL_Scancode key)
{
    switch (key) {
    case SDL_SCANCODE_Z:
        return 0;
    case SDL_SCANCODE_S:
        return 1;
    case SDL_SCANCODE_X:
        return 2;
    case SDL_SCANCODE_D:
        return 3;
    case SDL_SCANCODE_C:
        return 4;
    case SDL_SCANCODE_V:
        return 5;
    case SDL_SCANCODE_G:
        return 6;
    case SDL_SCANCODE_B:
        return 7;
    case SDL_SCANCODE_H:
        return 8;
    case SDL_SCANCODE_N:
        return 9;
    case SDL_SCANCODE_J:
        return 10;
    case SDL_SCANCODE_M:
        return 11;
    case SDL_SCANCODE_COMMA:
    case SDL_SCANCODE_Q:
        return 12;
    case SDL_SCANCODE_L:
    case SDL_SCANCODE_2:
        return 13;
    case SDL_SCANCODE_PERIOD:
    case SDL_SCANCODE_W:
        return 14;
    case SDL_SCANCODE_SEMICOLON:
    case SDL_SCANCODE_3:
        return 15;
    case SDL_SCANCODE_SLASH:
    case SDL_SCANCODE_E:
        return 16;
    case SDL_SCANCODE_R:
        return 17;
    case SDL_SCANCODE_5:
        return 18;
    case SDL_SCANCODE_T:
        return 19;
    case SDL_SCANCODE_6:
        return 20;
    case SDL_SCANCODE_Y:
        return 21;
    case SDL_SCANCODE_7:
        return 22;
    case SDL_SCANCODE_U:
        return 23;
    case SDL_SCANCODE_I:
        return 24;
    case SDL_SCANCODE_9:
        return 25;
    case SDL_SCANCODE_O:
        return 26;
    case SDL_SCANCODE_0:
        return 27;
    case SDL_SCANCODE_P:
        return 28;
    default:
        return -1;
    }
}

int App::sampleKeymap(SDL_Scancode key)
{
    switch (key) {
    case SDL_SCANCODE_KP_1:
        return 0;
    case SDL_SCANCODE_KP_2:
        return 1;
    case SDL_SCANCODE_KP_3:
        return 2;
    case SDL_SCANCODE_KP_4:
        return 3;
    case SDL_SCANCODE_KP_5:
        return 4;
    case SDL_SCANCODE_KP_6:
        return 5;
    case SDL_SCANCODE_KP_7:
        return 6;
    case SDL_SCANCODE_KP_8:
        return 7;
    case SDL_SCANCODE_KP_9:
        return 8;
    case SDL_SCANCODE_KP_0:
        return 9;
    default:
        return -1;
    }
}

} // namespace
