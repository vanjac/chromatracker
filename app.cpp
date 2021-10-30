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

const float CELL_HEIGHT = 32;
const float TRACK_SPACING = 80;
const float TRACK_WIDTH = 70;
const float PIANO_KEY_WIDTH = 40;
const int WHITE_KEYS[] = {0, 2, 4, 5, 7, 9, 11};
const int BLACK_KEYS[] = {1, 3, -1, 6, 8, 10, -1};

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
            + SDL_GetError() + "\n");
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
    song.tracks.reserve(8);
    for (int i = 0; i < 8; i++) {
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

    SDL_GetWindowSize(window, &winW, &winH);
    resizeWindow(winW, winH);

    SDL_PauseAudioDevice(audioDevice, 0); // audio devices start paused

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ui::FONT_DEFAULT.initGL();

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
                if (event.motion.state & SDL_BUTTON_LMASK != 0
                        && event.motion.x >= 0 && event.motion.x < winW) {
                    float volume = velocityToAmplitude(
                        (float)event.motion.x / winW);
                    if (!songVolumeOp) {
                        songVolumeOp = std::make_unique
                            <edit::ops::SetSongVolume>(volume);
                    } else {
                        songVolumeOp->undoIt(&song);
                        *songVolumeOp = edit::ops::SetSongVolume(volume);
                    }
                    songVolumeOp->doIt(&song); // preview before push undo stack
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
        drawInfo({{0, 0}, {winW - 180, 20}});
        if (browser) {
            browser->draw({{0, 20}, {winW - 180, winH - 100}});
        } else {
            drawTracks({{0, 20}, {winW - 180, 40}});
            drawEvents({{0, 40}, {winW - 180, winH - 100}}, playCur);
        }
        drawSampleList({{winW - 180, 0}, {winW, winH}});
        drawPiano({{0, winH - 100}, {winW - 180, winH}});
        glDisable(GL_SCISSOR_TEST);

        SDL_GL_SwapWindow(window);
    }
}

void App::resizeWindow(int w, int h)
{
    winW = w;
    winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void App::scissorRect(ui::Rect rect) const
{
    glScissor(rect.min.x, winH - rect.max.y, rect.width(), rect.height());
}

void App::drawInfo(ui::Rect rect)
{
    scissorRect(rect);

    glColor3f(1, 1, 1);
    glm::vec2 textPos = rect.min;
    textPos = ui::drawText("Record: ", textPos);
    textPos = ui::drawText(std::to_string(record), textPos);
    textPos = ui::drawText(" Follow: ", textPos);
    textPos = ui::drawText(std::to_string(followPlayback), textPos);
    textPos = ui::drawText(" Overwrite: ", textPos);
    textPos = ui::drawText(std::to_string(overwrite), textPos);
    textPos = ui::drawText("  ", textPos);

    std::shared_lock songLock(song.mu);
    float volumeX = textPos.x
        + amplitudeToVelocity(song.volume) * (rect.max.x - textPos.x);
    glColor3f(0, 0.7, 0);
    ui::drawRect({{textPos.x, rect.min.y}, {volumeX, rect.max.y}});
    glColor3f(0.2, 0.2, 0.2);
    ui::drawRect({{volumeX, rect.min.y}, {rect.max.x, rect.max.y}});
    glColor3f(1, 1, 1);
    ui::drawText("Volume", textPos);
}

void App::drawTracks(ui::Rect rect)
{
    scissorRect(rect);

    std::shared_lock songLock(song.mu);
    for (int i = 0; i < song.tracks.size(); i++) {
        auto track = song.tracks[i];
        std::shared_lock trackLock(track->mu);

        float x = rect.min.x + TRACK_SPACING * i;
        float xEnd = x + TRACK_WIDTH;
        float xCenter = (x + xEnd) / 2;
        glColor3f(0.2, 0.2, 0.2);
        ui::drawRect({{x, rect.min.y}, {xEnd, rect.max.y}});
        if (!track->mute) {
            float volumeX = x + amplitudeToVelocity(track->volume) * (xEnd - x);
            float panX = xCenter + track->pan * (xEnd - x) / 2;
            glColor3f(0, 0.7, 0);
            ui::drawRect({{x, rect.min.y}, {volumeX, rect.center().y}});
            ui::drawRect({{xCenter, rect.center().y}, {panX, rect.max.y}});

            glColor3f(1, 1, 1);
            glBegin(GL_LINES);
            glVertex2f(xCenter, rect.center().y);
            glVertex2f(xCenter, rect.max.y);
            glEnd();
        }

        glColor3f(1, 1, 1);
        ui::drawText(std::to_string(i + 1), {x + 20, rect.min.y});
    }
}

void App::drawEvents(ui::Rect rect, Cursor playCur)
{
    scissorRect(rect);

    float timeScale = CELL_HEIGHT / cellSize;

    static std::unordered_map<shared_ptr<const Section>, float>
        sectionPositions;
    sectionPositions.clear();
    {
        float y = 0;
        std::shared_lock songLock(song.mu);
        for (auto &section : song.sections) {
            sectionPositions[section] = y;
            std::shared_lock sectionLock(section->mu);
            y += section->length * timeScale + 48;
        }
    }

    float scroll = rect.center().y;
    if (auto sectionP = editCur.cursor.section.lock()) {
        scroll -= sectionPositions[sectionP] + editCur.cursor.time * timeScale;
    }

    {
        std::shared_lock songLock(song.mu);
        int curMeter = Section::NO_METER;
        for (auto &section : song.sections) {
            std::shared_lock sectionLock(section->mu);
            if (section->meter != Section::NO_METER)
                curMeter = section->meter;
            float sectionY = sectionPositions[section] + scroll;
            float sectionYEnd = sectionY + section->length * timeScale;
            if (sectionYEnd < rect.min.y || sectionY >= rect.max.y)
                continue;

            glColor3f(1, 1, 1);
            glm::ivec2 textPos {rect.min.x, sectionY - 20};
            textPos = ui::drawText(section->title, textPos);
            if (section->tempo != Section::NO_TEMPO) {
                textPos = ui::drawText("  Tempo=", textPos);
                textPos = ui::drawText(std::to_string(section->tempo), textPos);
            }
            if (section->meter != Section::NO_METER) {
                textPos = ui::drawText("  Meter=", textPos);
                textPos = ui::drawText(std::to_string(section->meter), textPos);
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

                    float x = t * TRACK_SPACING + rect.min.x;
                    float xEnd = x + TRACK_WIDTH;
                    float y = event.time * timeScale + sectionY;
                    float yEnd = nextEventTime * timeScale + sectionY;
                    glm::vec3 color = mute ? glm::vec3{0.3, 0.3, 0.3}
                        : glm::vec3{0.5, 0, 0.2};
                    if (!curSample)
                        color = glm::vec3(0);
                    else
                        color *= curVelocity; // TODO gamma correct
                    glColor3f(color.r, color.g, color.b);
                    ui::drawRect({{x, y}, {xEnd, yEnd}});
                    glColor3f(1, 1, 1);
                    glBegin(GL_LINES);
                    glVertex2f(x, y);
                    glVertex2f(xEnd, y);
                    glEnd();

                    // TODO avoid allocation
                    glm::ivec2 textPos {x, y};
                    if (sampleP) {
                        textPos = ui::drawText(sampleP->name.substr(0, 2),
                                               textPos);
                    } else {
                        textPos = ui::drawText("  ", textPos);
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
                    textPos = ui::drawText(specialStr, textPos);
                    if (event.pitch != Event::NO_PITCH) {
                        textPos = ui::drawText(pitchToString(event.pitch),
                                               textPos);
                    }
                } // each event
            } // each track
            
            glEnable(GL_BLEND);
            glBegin(GL_LINES);
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
                float y = grid * timeScale + sectionY;
                glVertex2f(rect.min.x, y);
                glVertex2f(rect.max.x, y);
            }
            glVertex2f(rect.min.x, sectionYEnd);
            glVertex2f(rect.max.x, sectionYEnd);
            glEnd();
            glDisable(GL_BLEND);
        } // each section
    } // songLock

    if (editCur.cursor.section.lock()) {
        glColor3f(0.5, 1, 0.5);
        glBegin(GL_LINES);
        glVertex2f(rect.min.x, rect.center().y);
        glVertex2f(rect.max.x, rect.center().y);
        glEnd();

        float cellX = editCur.track * TRACK_SPACING + rect.min.x;
        float cellY = rect.center().y;
        glColor4f(1, 1, 1, 0.5);
        glEnable(GL_BLEND);
        ui::drawRect({{cellX, cellY},
                      {cellX + TRACK_WIDTH, cellY + CELL_HEIGHT}});
        glDisable(GL_BLEND);
    }

    if (auto sectionP = playCur.section.lock()) {
        float playSectionY = sectionPositions[sectionP] + scroll;
        float playCursorY = playCur.time * timeScale + playSectionY;
        glColor3f(0.5, 0.5, 1);
        glBegin(GL_LINES);
        glVertex2f(rect.min.x, playCursorY);
        glVertex2f(rect.max.x, playCursorY);
        glEnd();
    }
}

void App::drawSampleList(ui::Rect rect)
{
    scissorRect(rect);
    std::shared_lock songLock(song.mu);
    for (int i = 0; i < song.samples.size(); i++) {
        if (i == selectedSample)
            glColor3f(0.7, 1.0, 0.7);
        else
            glColor3f(1, 1, 1);
        glm::vec2 textPos = rect.min;
        textPos.y += i * 20;
        ui::drawText(song.samples[i]->name, textPos);
    }
}

void App::drawPiano(ui::Rect rect)
{
    scissorRect(rect);

    int numWhiteKeys = rect.width() / PIANO_KEY_WIDTH + 1;
    for (int i = 0; i < numWhiteKeys; i++) {
        int key = WHITE_KEYS[i % 7];
        int octave = selectedOctave + (i / 7);
        int pitch = key + octave * OCTAVE;

        float x = rect.min.x + i * PIANO_KEY_WIDTH;
        float xEnd = x + PIANO_KEY_WIDTH;

        if (pitch == selectedPitch)
            glColor3f(0.7, 1.0, 0.7);
        else
            glColor3f(1, 1, 1);
        ui::drawRect({{x, rect.min.y}, {xEnd, rect.max.y}});

        glColor3f(0, 0, 0);
        glBegin(GL_LINES);
        glVertex2f(xEnd, rect.min.y);
        glVertex2f(xEnd, rect.max.y);
        glEnd();

        if (key == 0) {
            ui::drawText(std::to_string(octave), {x + 8, rect.max.y - 24});
        }
    }

    float blackYEnd = rect.min.y + rect.height() * 0.6;
    for (int i = 0; i < numWhiteKeys; i++) {
        int key = BLACK_KEYS[i % 7];
        if (key < 0)
            continue;
        int pitch = key + (selectedOctave + (i / 7)) * OCTAVE;

        float x = rect.min.x + (i + 0.75) * PIANO_KEY_WIDTH;
        if (pitch == selectedPitch)
            glColor3f(0, 0.7, 0);
        else
            glColor3f(0, 0, 0);
        ui::drawRect({{x, rect.min.y}, {x + PIANO_KEY_WIDTH / 2, blackYEnd}});
    }
}

void App::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    bool shift = e.keysym.mod & KMOD_SHIFT;
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
            browser = std::make_unique<ui::panels::Browser>(this,
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
                    selectedSample = numSamples;

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        } else {
            std::shared_lock songLock(song.mu);
            selectedSample++;
            if (selectedSample >= song.samples.size())
                selectedSample = song.samples.size() - 1;
        }
        break;
    case SDLK_KP_MINUS:
        if (ctrl) {
            shared_ptr<Sample> sample;
            {
                std::shared_lock lock(song.mu);
                if (selectedSample >= 0 && selectedSample < song.samples.size())
                    sample = song.samples[selectedSample];
                if (selectedSample == song.samples.size() - 1)
                    selectedSample--;
            }
            if (sample) {
                auto op = std::make_unique<edit::ops::DeleteSample>(sample);
                doOperation(std::move(op));
            }
        }
        selectedSample--;
        if (selectedSample < 0)
            selectedSample = 0;
        break;
    case SDLK_KP_MULTIPLY:
        {
            std::shared_lock songLock(song.mu);
            if (selectedSample < song.samples.size() - 10)
                selectedSample += 10;
        }
        break;
    case SDLK_KP_DIVIDE:
        selectedSample -= 10;
        if (selectedSample < 0)
            selectedSample = 0;
        break;
    case SDLK_EQUALS:
        if (selectedOctave < 9) {
            selectedOctave++;
            selectedPitch += 12;
        }
        break;
    case SDLK_MINUS:
        if (selectedOctave > 0) {
            selectedOctave--;
            selectedPitch -= 12;
        }
        break;
    /* File */
    case SDLK_o:
        if (ctrl) {
            browser = std::make_unique<ui::panels::Browser>(this,
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
        keyDownEvents(e, ctrl, shift);
    }
}

void App::keyDownEvents(const SDL_KeyboardEvent &e, bool ctrl, bool shift)
{
    if (!e.repeat && !ctrl) {
        int pitch = pitchKeymap(e.keysym.scancode);
        int sample = sampleKeymap(e.keysym.scancode);
        bool pick = e.keysym.sym == SDLK_RETURN;
        if (pitch >= 0) {
            selectedPitch = pitch + selectedOctave * OCTAVE;
        } else if (sample >= 0) {
            selectedSample = sample + (selectedSample / 10) * 10;
        } else if (pick) {
            pick = false;
            if (auto sectionP = editCur.cursor.section.lock()) {
                std::shared_lock sectionLock(sectionP->mu);
                auto eventIt = editCur.findEvent();
                if (eventIt != editCur.events().end()) {
                    if (eventIt->pitch != Event::NO_PITCH) {
                        selectedPitch = eventIt->pitch;
                        selectedOctave = selectedPitch / OCTAVE;
                        pick = true;
                    }
                    if (auto sampleP = eventIt->sample.lock()) {
                        std::shared_lock songLock(song.mu);
                        selectedSample = std::find(song.samples.begin(),
                            song.samples.end(), sampleP) - song.samples.begin();
                        pick = true;
                    }
                }
            }
        }
        if (pitch >= 0 || sample >= 0 || pick) {
            play::JamEvent jam;
            {
                std::shared_lock songLock(song.mu);
                if (selectedSample >= 0 && selectedSample < song.samples.size())
                    jam.event.sample = song.samples[selectedSample];
            }
            if (jam.event.sample.lock()) {
                jam.event.pitch = selectedPitch;
                jam.event.velocity = 1;
                jam.event.time = calcTickDelay(e.timestamp);
                jam.touchId = e.keysym.scancode;
                bool isPlaying;
                {
                    std::unique_lock playerLock(player.mu);
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
        } else {
            auto op = std::make_unique<edit::ops::ClearCell>(
                editCur, overwrite ? cellSize : 1);
            doOperation(std::move(op));
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
        jam.event.time = calcTickDelay(e.timestamp);
        jam.touchId = e.keysym.scancode;
        bool isPlaying;
        {
            std::unique_lock playerLock(player.mu);
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
