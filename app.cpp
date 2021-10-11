#include "app.h"
#include "file/itloader.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <unordered_map>
#include <glad/glad.h>

namespace chromatracker {

const float TIME_SCALE = 0.5;
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
        editCursor = Cursor(&song, song.sections[0].get());
    } else {
        editCursor = Cursor(&song);
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
        movedEditCursor = false;

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
                editCursor.move(-event.wheel.y * 12, Cursor::Space::Song);
                movedEditCursor = true;
                break;
            case SDL_MOUSEMOTION:
                if (event.motion.state & SDL_BUTTON_LMASK) {
                    float vel = (event.motion.x / 800.0f);
                    if (vel > 1) vel = 1;
                    if (vel < 0) vel = 0;
                    std::unique_lock songLock(song.mu);
                    song.volume = velocityToAmplitude(vel);
                    song.volume *= 0.5;
                }
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        sectionPositions.clear();
        {
            float y = 0;
            std::shared_lock songLock(song.mu);
            for (auto &section : song.sections) {
                sectionPositions[section.get()] = y;
                std::shared_lock sectionLock(section->mu);
                y += section->length * TIME_SCALE + 48;
            }
        }

        Cursor playCursor;
        {
            std::unique_lock lock(player.mu);
            playCursor = player.cursor();
            if (followPlayback && playCursor.valid()) {
                if (movedEditCursor) {
                    playCursor = editCursor;
                    player.setCursor(playCursor);
                } else {
                    editCursor = playCursor;
                }
            }
        }
        float scroll = winH / 2;
        if (editCursor.valid()) {
            scroll -= sectionPositions[editCursor.section]
                + editCursor.time * TIME_SCALE;
        }

        {
            std::shared_lock songLock(song.mu);
            for (auto &section : song.sections) {
                std::shared_lock sectionLock(section->mu);
                float sectionY = sectionPositions[section.get()] + scroll;
                float sectionYEnd = sectionY + section->length * TIME_SCALE;
                if (sectionYEnd < 0 || sectionY >= winH)
                    continue;

                glColor3f(1, 1, 1);
                text.drawText(section->title, {0, sectionY - 20});
                for (int t = 0; t < section->trackEvents.size(); t++) {
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
                        float y = event.time * TIME_SCALE + sectionY;
                        float yEnd = nextEventTime * TIME_SCALE + sectionY;
                        glm::vec3 color {0.5, 0, 0.2};
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
            } // each section
        } // songLock

        if (editCursor.valid()) {
            glColor3f(0.5, 1, 0.5);
            glBegin(GL_LINES);
            glVertex2f(0, winH / 2);
            glVertex2f(winW, winH / 2);
            glEnd();

            float selectedTrackX = selectedTrack * TRACK_SPACING;
            float selectedTrackXEnd = selectedTrackX + TRACK_WIDTH;
            glColor3f(1, 0.3, 1);
            glBegin(GL_QUADS);
            glVertex2f(selectedTrackX, winH / 2 - 2);
            glVertex2f(selectedTrackX, winH / 2 + 2);
            glVertex2f(selectedTrackXEnd, winH / 2 + 2);
            glVertex2f(selectedTrackXEnd, winH / 2 - 2);
            glEnd();
        }

        if (playCursor.valid()) {
            float playSectionY = sectionPositions[playCursor.section] + scroll;
            float playCursorY = playCursor.time * TIME_SCALE + playSectionY;
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
    if (e.repeat)
        return;

    switch (e.keysym.sym) {
    case SDLK_F1:
        followPlayback = !followPlayback;
        break;
    case SDLK_SPACE:
        {
            std::unique_lock playerLock(player.mu);
            std::shared_lock songLock(song.mu);
            if (player.cursor().valid()) {
                player.fadeAll();
            } else if (!song.sections.empty()) {
                player.setCursor(editCursor);
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
        {
            std::unique_lock songLock(song.mu);
            if (!song.sections.empty()) {
                editCursor = Cursor(&song, song.sections[0].get());
            } else {
                editCursor = Cursor(&song);
            }
            movedEditCursor = true;
        }
        break;
    case SDLK_KP_PLUS:
        selectedSample++;
        cout << "Sample " <<selectedSample<< "\n";
        break;
    case SDLK_KP_MINUS:
        selectedSample--;
        cout << "Sample " <<selectedSample<< "\n";
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
        selectedTrack++;
        break;
    case SDLK_LEFT:
        selectedTrack--;
        break;
    case SDLK_F5:
        {
            std::unique_lock songLock(song.mu);
            if (selectedTrack >= 0 && selectedTrack < song.tracks.size()) {
                Track *track = song.tracks[selectedTrack].get();
                std::unique_lock trackLock(track->mu);
                track->mute = !track->mute;
            }
        }
        break;
    default:
        int key = pitchKeymap(e.keysym.sym);
        if (key >= 0) {
            std::unique_lock songLock(song.mu);
            if (selectedSample >= 0 && selectedSample < song.samples.size()) {
                std::unique_lock playerLock(player.mu);
                play::JamEvent jam;
                jam.event.sample = song.samples[selectedSample].get();
                jam.event.pitch = key + selectedOctave * OCTAVE;
                jam.event.velocity = 1;
                jam.event.time = calcTickDelay(e.timestamp);
                jam.touchId = key;
                player.queueJamEvent(jam);
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
