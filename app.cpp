#include "app.h"
#include "edit/songops.h"
#include "file/itloader.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <unordered_map>
#include <glad/glad.h>

namespace chromatracker {

const float CELL_HEIGHT = 32;
const float TRACK_SPACING = 100;
const float TRACK_WIDTH = 80;

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
    if (args.size() < 2) {
        throw std::runtime_error("please specify file path :)\n");
    }

    SDL_RWops *stream = SDL_RWFromFile(args[1].c_str(), "r");
    if (!stream)
        throw std::runtime_error(string("Error opening stream: ")
            + SDL_GetError() + "\n");

    file::ITLoader loader(stream);
    loader.loadSong(&song);

    cout <<song.tracks.size()<< " tracks\n";

    // don't need locks at this moment
    player.setCursor(Cursor(&song));
    if (!song.sections.empty()) {
        editCur.cursor = Cursor(&song, song.sections[0]);
    } else {
        editCur.cursor = Cursor(&song);
    }

    SDL_GetWindowSize(window, &winW, &winH);
    resizeWindow(winW, winH);

    SDL_PauseAudioDevice(audioDevice, 0); // audio devices start paused

    glClearColor(0, 0, 0, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    text.initGL();

    std::unordered_map<shared_ptr<const Section>, float> sectionPositions;

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
                keyUp(event.key);
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
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        float timeScale = CELL_HEIGHT / cellSize;

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
        float scroll = winH / 2;
        if (auto sectionP = editCur.cursor.section.lock()) {
            scroll -= sectionPositions[sectionP]
                + editCur.cursor.time * timeScale;
        }

        {
            std::shared_lock songLock(song.mu);
            for (auto &section : song.sections) {
                std::shared_lock sectionLock(section->mu);
                float sectionY = sectionPositions[section] + scroll;
                float sectionYEnd = sectionY + section->length * timeScale;
                if (sectionYEnd < 0 || sectionY >= winH)
                    continue;

                glColor3f(1, 1, 1);
                text.drawText(section->title, {0, sectionY - 20});
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

                        float x = t * TRACK_SPACING;
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
                        glBegin(GL_QUADS);
                        glVertex2f(x, y);
                        glVertex2f(x, yEnd);
                        glVertex2f(xEnd, yEnd);
                        glVertex2f(xEnd, y);
                        glEnd();
                        glColor3f(1, 1, 1);
                        glBegin(GL_LINES);
                        glVertex2f(x, y);
                        glVertex2f(xEnd, y);
                        glEnd();

                        // TODO avoid allocation
                        glm::ivec2 textPos {x, y};
                        if (sampleP) {
                            textPos = text.drawText(sampleP->name.substr(0, 2), textPos);
                        } else {
                            textPos = text.drawText("  ", textPos);
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
                        textPos = text.drawText(specialStr, textPos);
                        if (event.pitch != Event::NO_PITCH) {
                            textPos = text.drawText(pitchToString(event.pitch), textPos);
                        }
                    } // each event
                } // each track
                
                glEnable(GL_BLEND);
                glColor4f(1, 1, 1, 0.4);
                glBegin(GL_LINES);
                for (ticks grid = 0; grid < section->length; grid += cellSize) {
                    float y = grid * timeScale + sectionY;
                    glVertex2f(0, y);
                    glVertex2f(winW, y);
                }
                glVertex2f(0, sectionYEnd);
                glVertex2f(winW, sectionYEnd);
                glEnd();
                glDisable(GL_BLEND);
            } // each section
        } // songLock

        if (editCur.cursor.section.lock()) {
            glColor3f(0.5, 1, 0.5);
            glBegin(GL_LINES);
            glVertex2f(0, winH / 2);
            glVertex2f(winW, winH / 2);
            glEnd();

            float cellX = editCur.track * TRACK_SPACING;
            float cellXEnd = cellX + TRACK_WIDTH;
            float cellY = winH / 2;
            float cellYEnd = cellY + CELL_HEIGHT;
            glColor4f(1, 1, 1, 0.5);
            glEnable(GL_BLEND);
            glBegin(GL_QUADS);
            glVertex2f(cellX, cellY);
            glVertex2f(cellX, cellYEnd);
            glVertex2f(cellXEnd, cellYEnd);
            glVertex2f(cellXEnd, cellY);
            glEnd();
            glDisable(GL_BLEND);
        }

        if (auto sectionP = playCur.section.lock()) {
            float playSectionY = sectionPositions[sectionP] + scroll;
            float playCursorY = playCur.time * timeScale + playSectionY;
            glColor3f(0.5, 0.5, 1);
            glBegin(GL_LINES);
            glVertex2f(0, playCursorY);
            glVertex2f(winW, playCursorY);
            glEnd();
        }

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

void App::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    bool shift = e.keysym.mod & KMOD_SHIFT;
    if (!e.repeat && !ctrl) {
        int pitch = pitchKeymap(e.keysym.scancode);
        int sample = sampleKeymap(e.keysym.scancode);
        if (pitch >= 0) {
            selectedPitch = pitch + selectedOctave * OCTAVE;
        } else if (sample >= 0) {
            selectedSample = sample + (selectedSample / 10) * 10;
            cout << "Sample " <<(selectedSample + 1)<< "\n";
        }
        if (pitch >= 0 || sample >= 0) {
            play::JamEvent jam;
            {
                std::unique_lock songLock(song.mu);
                if (selectedSample >= 0 && selectedSample < song.samples.size())
                    jam.event.sample = song.samples[selectedSample];
            }
            if (!jam.event.sample.expired()) {
                jam.event.pitch = selectedPitch;
                jam.event.velocity = 1;
                jam.event.time = calcTickDelay(e.timestamp);
                jam.touchId = e.keysym.scancode;
                bool isPlaying;
                {
                    std::unique_lock playerLock(player.mu);
                    player.queueJamEvent(jam);
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
    case SDLK_F1:
        if (e.repeat) break;
        record = !record;
        cout << "Record " <<record<< "\n";
        break;
    case SDLK_F4:
        if (e.repeat) break;
        followPlayback = !followPlayback;
        cout << "Follow " <<followPlayback<< "\n";
        break;
    case SDLK_F7:
        if (e.repeat) break;
        overwrite = !overwrite;
        cout << "Overwrite " <<overwrite<< "\n";
        break;
    case SDLK_KP_PLUS:
        selectedSample++;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_KP_MINUS:
        selectedSample--;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_KP_MULTIPLY:
        selectedSample += 10;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_KP_DIVIDE:
        selectedSample -= 10;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_EQUALS:
        selectedOctave++;
        selectedPitch += 12;
        cout << "Octave " <<selectedOctave<< "\n";
        break;
    case SDLK_MINUS:
        selectedOctave--;
        selectedPitch -= 12;
        cout << "Octave " <<selectedOctave<< "\n";
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
            std::unique_lock songLock(song.mu);
            if (!song.sections.empty()) {
                editCur.cursor.section = song.sections[0];
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
        if (ctrl) {
            std::unique_lock songLock(song.mu);
            editCur.track = song.tracks.size() - 1;
        } else {
            editCur.track++;
        }
        break;
    case SDLK_LEFT:
        if (ctrl) {
            editCur.track = 0;
        } else {
            editCur.track--;
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
        cellSize /= ctrl ? 3 : 2;
        break;
    /* Commands */
    case SDLK_m:
        if (!e.repeat && ctrl) {
            // NOT undoable!
            std::unique_lock songLock(song.mu);
            if (editCur.track >= 0 && editCur.track < song.tracks.size()) {
                auto track = song.tracks[editCur.track];
                std::unique_lock trackLock(track->mu);
                track->mute = !track->mute;
                cout << "Track " <<editCur.track<<
                    " mute " <<track->mute<< "\n";
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
        {
            Event fadeEvent;
            fadeEvent.special = Event::Special::FadeOut;
            auto op = std::make_unique<edit::ops::WriteCell>(
                editCur, overwrite ? cellSize : 1, fadeEvent);
            doOperation(std::move(op));
        }
    }
}

void App::keyUp(const SDL_KeyboardEvent &e)
{
    if (e.keysym.mod & KMOD_CTRL)
        return;
    int pitch = pitchKeymap(e.keysym.scancode);
    int sampleI = sampleKeymap(e.keysym.scancode);
    if (pitch >= 0 || sampleI >= 0) {
        play::JamEvent jam;
        jam.event.special = Event::Special::FadeOut;
        jam.event.time = calcTickDelay(e.timestamp);
        jam.touchId = e.keysym.scancode;
        bool isPlaying;
        {
            std::unique_lock playerLock(player.mu);
            player.queueJamEvent(jam);
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
