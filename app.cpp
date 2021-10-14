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
        editCur.cursor = Cursor(&song, song.sections[0].get());
    } else {
        editCur.cursor = Cursor(&song);
    }

    SDL_GetWindowSize(window, &winW, &winH);
    resizeWindow(winW, winH);

    SDL_PauseAudioDevice(audioDevice, 0); // audio devices start paused

    glClearColor(0, 0, 0, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    text.initGL();

    std::unordered_map<const Section *, float> sectionPositions;

    bool running = true;
    while (running) {
        movedEditCur = false;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
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
                editCur.cursor.move(-event.wheel.y * cellSize,
                                    Cursor::Space::Song);
                movedEditCur = true;
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
                sectionPositions[section.get()] = y;
                std::shared_lock sectionLock(section->mu);
                y += section->length * timeScale + 48;
            }
        }

        Cursor playCur;
        {
            std::unique_lock lock(player.mu);
            playCur = player.cursor();
            if (followPlayback && playCur.valid()) {
                if (movedEditCur) {
                    playCur = editCur.cursor;
                    player.setCursor(playCur);
                } else {
                    editCur.cursor = playCur;
                }
            }
        }
        float scroll = winH / 2;
        if (editCur.cursor.valid()) {
            scroll -= sectionPositions[editCur.cursor.section]
                + editCur.cursor.time * timeScale;
        }

        {
            std::shared_lock songLock(song.mu);
            for (auto &section : song.sections) {
                std::shared_lock sectionLock(section->mu);
                float sectionY = sectionPositions[section.get()] + scroll;
                float sectionYEnd = sectionY + section->length * timeScale;
                if (sectionYEnd < 0 || sectionY >= winH)
                    continue;

                glColor3f(1, 1, 1);
                text.drawText(section->title, {0, sectionY - 20});
                for (int t = 0; t < section->trackEvents.size(); t++) {
                    bool mute;
                    {
                        Track *track = song.tracks[t].get();
                        std::shared_lock trackLock(track->mu);
                        mute = track->mute;
                    }
                    auto &events = section->trackEvents[t];
                    Sample *curSample = nullptr;
                    float curVelocity = 1.0f;
                    for (int e = 0; e < events.size(); e++) {
                        const Event &event = events[e];
                        ticks nextEventTime = section->length;
                        if (e != events.size() - 1)
                            nextEventTime = events[e + 1].time;

                        if (event.special == Event::Special::FadeOut) {
                            curSample = nullptr; // TODO gradient
                        } else if (event.sample) {
                            curSample = event.sample;
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
                        if (event.sample) {
                            textPos = text.drawText(event.sample->name.substr(0, 2), textPos);
                        } else {
                            textPos = text.drawText("  ", textPos);
                        }
                        textPos.x += 12;
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

        if (editCur.cursor.valid()) {
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

        if (playCur.valid()) {
            float playSectionY = sectionPositions[playCur.section] + scroll;
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
    switch (e.keysym.sym) {
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
    case SDLK_SPACE:
        if (e.repeat) break;
        {
            std::unique_lock playerLock(player.mu);
            std::shared_lock songLock(song.mu);
            if (player.cursor().valid()) {
                player.fadeAll();
            } else if (!song.sections.empty()) {
                player.setCursor(editCur.cursor);
            }
        }
        break;
    case SDLK_ESCAPE:
        {
            std::unique_lock playerLock(player.mu);
            player.stop();
        }
        break;
    case SDLK_HOME:
        if (e.keysym.mod & KMOD_CTRL) {
            std::unique_lock songLock(song.mu);
            if (!song.sections.empty()) {
                editCur.cursor.section = song.sections[0].get();
            }
        }
        editCur.cursor.time = 0;
        movedEditCur = true;
        break;
    case SDLK_PAGEDOWN:
        {
            std::unique_lock songLock(song.mu);
            auto sectionIt = editCur.cursor.findSection();
            if (sectionIt != song.sections.end()) {
                sectionIt++;
            }
            if (sectionIt != song.sections.end()) {
                Section *section = sectionIt->get();
                editCur.cursor.section = section;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_PAGEUP:
        {
            std::unique_lock songLock(song.mu);
            auto sectionIt = editCur.cursor.findSection();
            if (sectionIt != song.sections.end()
                    && sectionIt != song.sections.begin()) {
                sectionIt--;
                Section *section = sectionIt->get();
                editCur.cursor.section = section;
                editCur.cursor.time = 0;
                movedEditCur = true;
            }
        }
        break;
    case SDLK_KP_PLUS:
        selectedSample++;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_KP_MINUS:
        selectedSample--;
        cout << "Sample " <<(selectedSample + 1)<< "\n";
        break;
    case SDLK_EQUALS:
        selectedOctave++;
        cout << "Octave " <<selectedOctave<< "\n";
        break;
    case SDLK_MINUS:
        selectedOctave--;
        cout << "Octave " <<selectedOctave<< "\n";
        break;
    case SDLK_RIGHT:
        if (e.keysym.mod & KMOD_CTRL) {
            std::unique_lock songLock(song.mu);
            editCur.track = song.tracks.size() - 1;
        } else {
            editCur.track++;
        }
        break;
    case SDLK_LEFT:
        if (e.keysym.mod & KMOD_CTRL) {
            editCur.track = 0;
        } else {
            editCur.track--;
        }
        break;
    case SDLK_DOWN:
        // TODO snap to grid
        editCur.cursor.move(cellSize, Cursor::Space::Song);
        movedEditCur = true;
        break;
    case SDLK_UP:
        editCur.cursor.move(-cellSize, Cursor::Space::Song);
        movedEditCur = true;
        break;
    case SDLK_RIGHTBRACKET:
        cellSize *= (e.keysym.mod & KMOD_CTRL) ? 3 : 2;
        break;
    case SDLK_LEFTBRACKET:
        cellSize /= (e.keysym.mod & KMOD_CTRL) ? 3 : 2;
        break;
    case SDLK_F10:
        if (e.repeat) break;
        {
            std::unique_lock songLock(song.mu);
            if (editCur.track >= 0 && editCur.track < song.tracks.size()) {
                Track *track = song.tracks[editCur.track].get();
                std::unique_lock trackLock(track->mu);
                track->mute = !track->mute;
                cout << "Track " <<editCur.track<<
                    " mute " <<track->mute<< "\n";
            }
        }
        break;
    case SDLK_DELETE:
        edit::ops::clearCell(&song, editCur, overwrite ? cellSize : 1);
        break;
    default:
        if (e.repeat) break;
        int key = pitchKeymap(e.keysym.sym);
        if (key >= 0) {
            play::JamEvent jam;
            {
                std::unique_lock songLock(song.mu);
                if (selectedSample >= 0 && selectedSample < song.samples.size())
                    jam.event.sample = song.samples[selectedSample].get();
            }
            if (jam.event.sample) {
                jam.event.pitch = key + selectedOctave * OCTAVE;
                jam.event.velocity = 1;
                jam.event.time = calcTickDelay(e.timestamp);
                jam.touchId = key;
                {
                    std::unique_lock playerLock(player.mu);
                    player.queueJamEvent(jam);
                }
                if (record) {
                    edit::ops::writeCell(&song, editCur,
                                         overwrite ? cellSize : 1, jam.event);
                }
            }
        }
    }
}

void App::keyUp(const SDL_KeyboardEvent &e)
{
    int key = pitchKeymap(e.keysym.sym);
    if (key >= 0) {
        std::unique_lock playerLock(player.mu);
        play::JamEvent jam;
        jam.event.special = Event::Special::FadeOut;
        jam.event.time = calcTickDelay(e.timestamp);
        jam.touchId = key;
        player.queueJamEvent(jam);
    }
}

ticks App::calcTickDelay(uint32_t timestamp) {
    timestamp -= audioCallbackTime;
    float ticksPerSecond = player.currentTempo() * TICKS_PER_BEAT / 60.0;
    return timestamp / 1000.0 * ticksPerSecond;
}

void cAudioCallback(void * userdata, uint8_t *stream, int len) {
    ((App *)userdata)->audioCallback(stream, len);
}

void App::audioCallback(uint8_t *stream, int len) {
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

int App::pitchKeymap(SDL_Keycode key) {
    switch(key) {
    case SDLK_z:
        return 0;
    case SDLK_s:
        return 1;
    case SDLK_x:
        return 2;
    case SDLK_d:
        return 3;
    case SDLK_c:
        return 4;
    case SDLK_v:
        return 5;
    case SDLK_g:
        return 6;
    case SDLK_b:
        return 7;
    case SDLK_h:
        return 8;
    case SDLK_n:
        return 9;
    case SDLK_j:
        return 10;
    case SDLK_m:
        return 11;
    case SDLK_COMMA:
    case SDLK_q:
        return 12;
    case SDLK_l:
    case SDLK_2:
        return 13;
    case SDLK_PERIOD:
    case SDLK_w:
        return 14;
    case SDLK_SEMICOLON:
    case SDLK_3:
        return 15;
    case SDLK_SLASH:
    case SDLK_e:
        return 16;
    case SDLK_r:
        return 17;
    case SDLK_5:
        return 18;
    case SDLK_t:
        return 19;
    case SDLK_6:
        return 20;
    case SDLK_y:
        return 21;
    case SDLK_7:
        return 22;
    case SDLK_u:
        return 23;
    case SDLK_i:
        return 24;
    case SDLK_9:
        return 25;
    case SDLK_o:
        return 26;
    case SDLK_0:
        return 27;
    case SDLK_p:
        return 28;
    default:
        return -1;
    }
}

} // namespace
