#include "app.h"
#include "edit/songops.h"
#include "file/chromawriter.h"
#include <stdexcept>
#include <glad/glad.h>

namespace chromatracker {

using namespace ui;

void cAudioCallback(void * userdata, uint8_t *stream, int len);

App::App(SDL_Window *window)
    : window(window)
    , eventKeyboard(this)
    , eventsEdit(this)
    , sampleEdit(this)
{
    // TODO
    settings.bookmarks.push_back("D:\\Google Drive\\mods");
    settings.bookmarks.push_back("D:\\Google Drive\\mods\\downloaded");

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
        section->trackEvents.resize(song.tracks.size());
        section->tempo = 125;
        section->meter = 4;
    }

    undoer.reset(&song);
    eventsEdit.resetCursor(true);
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
                    if (tab == Tab::Events) {
                        eventsEdit.keyUp(event.key);
                    }
                    eventKeyboard.keyUp(event.key);
                }
                break;
            case SDL_MOUSEWHEEL:
                if (!browser && tab == Tab::Events) {
                    eventsEdit.mouseWheel(event.wheel);
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

        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);

        float lineHeight = FONT_DEFAULT.lineHeight;
        songEdit.draw(this, {winR(TL), winR(TR, {-160, lineHeight})}, &song);
        Rect mainR {winR(TL, {0, lineHeight}), winR(BR, {-160, -100})};
        if (browser) {
            browser->draw(mainR);
        } else if (tab == Tab::Events) {
            eventsEdit.draw(mainR);
        } else if (tab == Tab::Sample) {
            sampleEdit.draw(mainR);
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

void App::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    switch (e.keysym.sym) {
    case SDLK_z:
        if (ctrl) {
            undoer.undo();
        }
        break;
    case SDLK_y:
        if (ctrl) {
            undoer.redo();
        }
        break;
    /* Tabs */
    case SDLK_F2:
        tab = Tab::Events;
        break;
    case SDLK_F3:
        tab = Tab::Sample;
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
                    undoer.doOp(edit::ops::AddSample(numSamples, newSample));
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
                undoer.doOp(edit::ops::DeleteSample(sampleP));
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
                    }
                    undoer.reset(&song);
                    eventsEdit.resetCursor(true);
                    eventKeyboard.reset();

                    // call at the end to prevent access violation!
                    browser.reset();
                });
        }
        break;
    case SDLK_s:
        if (ctrl) {
            SDL_RWops *stream = SDL_RWFromFile("out.chroma", "w");
            if (!stream) {
                cout << "Error opening stream: " <<SDL_GetError()<< "\n";
                break;
            }
            file::chroma::Writer writer(stream);
            writer.writeSong(&song);
        }
        break;
    }

    if (browser) {
        browser->keyDown(e);
    } else if (tab == Tab::Events) {
        eventsEdit.keyDown(e);
    }
    eventKeyboard.keyDown(e);
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
