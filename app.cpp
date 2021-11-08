#include "app.h"
#include "ui/draw.h"
#include "ui/text.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
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
            case SDL_MOUSEMOTION:
                if (event.motion.state & SDL_BUTTON_LMASK != 0) {
                    if (selectedEvent.velocity == Event::NO_VELOCITY)
                        selectedEvent.velocity = 1;
                    selectedEvent.velocity -=
                        (float)event.motion.yrel * 2 / winR.dim().y;
                    if (selectedEvent.velocity > 1) {
                        selectedEvent.velocity = 1;
                    } else if (selectedEvent.velocity < 0) {
                        selectedEvent.velocity = 0;
                    }
                    // float volume = velocityToAmplitude(
                    //     winR.normalized({event.motion.x, 0}).x);
                    // if (!songVolumeOp) {
                    //     songVolumeOp = std::make_unique
                    //         <edit::ops::SetSongVolume>(volume);
                    // } else {
                    //     songVolumeOp->undoIt(&song);
                    //     *songVolumeOp = edit::ops::SetSongVolume(volume);
                    // }
                    // songVolumeOp->doIt(&song); // preview before push undo stack
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (songVolumeOp) {
                    songVolumeOp->undoIt(&song);
                    doOperation(std::move(songVolumeOp));
                    // songVolumeOp will be null after move
                }
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
    textPos = drawText("Record: ", textPos);
    textPos = drawText(std::to_string(record), textPos);
    textPos = drawText(" Follow: ", textPos);
    textPos = drawText(std::to_string(followPlayback), textPos);
    textPos = drawText(" Overwrite: ", textPos);
    textPos = drawText(std::to_string(overwrite), textPos);
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
                    drawRect(Rect::hLine(eventR(TL), eventR.right(), 1));

                    // TODO avoid allocation
                    glm::vec2 textPos = eventR(TL, {2, 0});
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
            } else {
                cout << "Nothing to redo\n";
            }
        }
        break;
    /* Mode */
    case SDLK_F5:
        if (e.repeat) break;
        record = !record;
        break;
    case SDLK_F6:
        if (e.repeat) break;
        followPlayback = !followPlayback;
        if (!followPlayback)
            snapToGrid();
        break;
    case SDLK_F7:
        if (e.repeat) break;
        overwrite = !overwrite;
        break;
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
    case SDLK_EQUALS:
        if (selectedOctave < 9) {
            selectedOctave++;
            if (selectedEvent.pitch != Event::NO_PITCH)
                selectedEvent.pitch += 12;
        }
        break;
    case SDLK_MINUS:
        if (selectedOctave > 0) {
            selectedOctave--;
            if (selectedEvent.pitch != Event::NO_PITCH)
                selectedEvent.pitch -= 12;
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
        bool pick = e.keysym.sym == SDLK_RETURN;
        if (pitch >= 0) {
            selectedEvent.pitch = pitch + selectedOctave * OCTAVE;
        } else if (sample >= 0) {
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
        } else if (pick) {
            pick = false;
            if (auto sectionP = editCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = editCur.findEvent();
                if (eventIt != editCur.events().end()) {
                    // mirrors TrackPlay::processEvent
                    if (auto sampleP = eventIt->sample.lock())
                        selectedEvent.sample = sampleP;
                    if (eventIt->pitch != Event::NO_PITCH) {
                        selectedEvent.pitch = eventIt->pitch;
                        selectedOctave = selectedEvent.pitch / OCTAVE;
                    }
                    if (eventIt->velocity != Event::NO_VELOCITY)
                        selectedEvent.velocity = eventIt->velocity;
                    selectedEvent.special = eventIt->special;
                    pick = true;
                }
            }
        }
        if (pitch >= 0 || sample >= 0 || pick) {
            play::JamEvent jam;
            jam.event = selectedEvent;
            if (jam.event.sample.lock()) {
                jam.touchId = e.keysym.scancode;
                bool isPlaying;
                {
                    std::unique_lock playerLock(player.mu);
                    jam.event.time = calcTickDelay(e.timestamp);
                    player.jam.queueJamEvent(jam);
                    isPlaying = (bool)player.cursor().section.lock();
                }
                if (record && pitch >= 0) {
                    auto op = std::make_unique<edit::ops::WriteCell>(editCur,
                        (overwrite && !isPlaying) ? cellSize : 1, jam.event);
                    doOperation(std::move(op));
                    // TODO if overwrite && isPlaying, clear events while held
                    // TODO combine into single undo operation while playing
                }
            }
            return;
        }
    }

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
        nextCell();
        break;
    case SDLK_UP:
        prevCell();
        break;
    case SDLK_RIGHTBRACKET:
        cellSize *= ctrl ? 3 : 2;
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
        if (!e.repeat && ctrl) {
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
        } else if (alt) {
            {
                std::shared_lock lock(song.mu);
                if (editCur.track >= song.tracks.size()) {
                    editCur.track = song.tracks.size() - 1;
                } else if (editCur.track < 0) {
                    editCur.track = 0;
                }
            }
            shared_ptr<Track> newTrack(new Track);
            auto op = std::make_unique<edit::ops::AddTrack>(
                editCur.track + 1, newTrack);
            doOperation(std::move(op));
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
        } else if (alt) {
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
        } else {
            auto op = std::make_unique<edit::ops::ClearCell>(
                editCur, overwrite ? cellSize : 1);
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
    case SDLK_BACKQUOTE:
        if (record) {
            Event fadeEvent;
            fadeEvent.special = Event::Special::FadeOut;
            auto op = std::make_unique<edit::ops::WriteCell>(
                editCur, overwrite ? cellSize : 1, fadeEvent);
            doOperation(std::move(op));
        }
    }
}

void App::keyUpEvents(const SDL_KeyboardEvent &e)
{
    if (e.keysym.mod & KMOD_CTRL)
        return;
    int pitch = pitchKeymap(e.keysym.scancode);
    int sampleI = sampleKeymap(e.keysym.scancode);
    if (pitch >= 0 || sampleI >= 0 || e.keysym.sym == SDLK_RETURN) {
        play::JamEvent jam;
        jam.event.special = Event::Special::FadeOut;
        jam.touchId = e.keysym.scancode;
        bool isPlaying;
        {
            std::unique_lock playerLock(player.mu);
            jam.event.time = calcTickDelay(e.timestamp);
            player.jam.queueJamEvent(jam);
            isPlaying = (bool)player.cursor().section.lock();
        }
        if (record && isPlaying && pitch >= 0) {
            auto op = std::make_unique<edit::ops::WriteCell>(
                editCur, 1, jam.event);
            doOperation(std::move(op));
        }
    }
}

void App::doOperation(unique_ptr<edit::SongOp> op)
{
    if (op->doIt(&song)) {
        undoStack.push_back(std::move(op));
        redoStack.clear();
    }
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
