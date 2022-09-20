#include "eventkeyboard.h"
#include <app.h>
#include <edit/editor.h>
#include <algorithm>

namespace chromatracker::ui::panels {

const float PIANO_KEY_WIDTH = 40;
const int WHITE_KEYS[] = {0, 2, 4, 5, 7, 9, 11};
const int BLACK_KEYS[] = {-1, 1, 3, -1, 6, 8, 10};

EventKeyboard::EventKeyboard(App *app)
    : app(app)
    , editor(&app->editor)
{}

void EventKeyboard::drawPiano(Rect rect)
{
    app->scissorRect(rect);
    Event &selected = editor->selected;

    int numWhiteKeys = rect.dim().x / PIANO_KEY_WIDTH + 1;
    for (int i = 0; i < numWhiteKeys; i++) {
        int key = WHITE_KEYS[i % 7];
        int o = octave + (i / 7);
        int pitch = key + o * OCTAVE;

        Rect keyR = Rect::from(TL, rect(TL, {PIANO_KEY_WIDTH * i, 0}),
                               {PIANO_KEY_WIDTH, rect.dim().y});
        drawRect(keyR, pitch == selected.pitch ? C_ACCENT_LIGHT : C_WHITE);
        drawRect(Rect::vLine(keyR(TL), keyR.bottom(), 1), C_BLACK);
        if (key == 0) {
            drawText(std::to_string(o), keyR(BL, {8, -24}), C_BLACK);
        }
    }

    for (int i = 0; i < numWhiteKeys + 1; i++) {
        int key = BLACK_KEYS[i % 7];
        if (key < 0)
            continue;
        int pitch = key + (octave + (i / 7)) * OCTAVE;

        drawRect(Rect::from(TC, rect(TL, {i * PIANO_KEY_WIDTH, 0}),
                            {PIANO_KEY_WIDTH / 2, rect.dim().y * 0.6}),
                 pitch == selected.pitch ? C_ACCENT : C_BLACK);
    }

    Rect velocityR = Rect::from(TR, rect(TR), {16, rect.dim().y});

    if (velocityTouch.expired())
        velocityTouch = app->captureTouch(velocityR);
    auto touch = velocityTouch.lock();
    if (touch) {
        for (auto &event : touch->events) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                play::JamEvent jam {selected, (int)event.button.which + 1};
                bool playing = app->jamEvent(jam, event.button.timestamp);
                editor->writeEvent(playing, jam.event, Event::VELOCITY,
                                   !playing); // continuous if not playing
            } else if (event.type == SDL_MOUSEMOTION) {
                if (selected.velocity == Event::NO_VELOCITY)
                    selected.velocity = 1;
                selected.velocity -=
                    (float)event.motion.yrel * 1.5 / velocityR.dim().y;
                selected.velocity = glm::clamp(selected.velocity, 0.0f, 1.0f);
                play::JamEvent jam {selected.masked(Event::VELOCITY),
                                    (int)event.button.which + 1};
                bool playing = app->jamEvent(jam, event.button.timestamp);
                editor->writeEvent(playing, playing ? jam.event : selected,
                                   Event::VELOCITY, !playing);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                Event fadeEvent;
                fadeEvent.special = Event::Special::FadeOut;
                if (app->jamEvent({fadeEvent, (int)event.button.which + 1},
                                  event.button.timestamp)) // if playing
                    editor->writeEvent(true, fadeEvent, Event::ALL);
                editor->undoer.endContinuous();
            }
        }
        touch->events.clear();
    }

    if (selected.velocity != Event::NO_VELOCITY) {
        drawRect(velocityR,
                 C_DARK_GRAY * (touch ? SELECT_COLOR : NORMAL_COLOR));
        drawRect({velocityR({0, 1 - selected.velocity}),  velocityR(BR)},
                 C_ACCENT * (touch ? SELECT_COLOR : NORMAL_COLOR));
    }
}

void EventKeyboard::drawSampleList(Rect rect)
{
    app->scissorRect(rect);
    std::shared_lock songLock(editor->song.mu);
    auto selectedSampleP = editor->selected.sample.lock();
    glm::vec2 textPos = rect(TL);
    for (const auto &sample : editor->song.samples) {
        Rect sampleR = Rect::from(TL, textPos,
                                  {rect.dim().x, FONT_DEFAULT.lineHeight});
        if (sample == selectedSampleP)
            drawRect(sampleR, glm::vec4(sample->color * 0.25f, 1));
        textPos = drawText(sample->name, textPos, sample == selectedSampleP ?
            C_WHITE : glm::vec4(sample->color * 0.5f + 0.5f, 1))(BL);
    }
}

void EventKeyboard::keyDown(const SDL_KeyboardEvent &e)
{
    bool ctrl = e.keysym.mod & KMOD_CTRL;
    Song &song = editor->song;
    Event &selected = editor->selected;

    if (!e.repeat && !ctrl) {
        int pitch = pitchKeymap(e.keysym.scancode);
        int sample = sampleKeymap(e.keysym.scancode);
        Event::Mask mask = Event::NO_MASK;
        if (pitch >= 0) {
            mask = Event::PITCH;
            selected.pitch = pitch + octave * OCTAVE;
        } else if (sample >= 0) {
            mask = Event::SAMPLE;
            std::shared_lock songLock(song.mu);
            int index = editor->selectedSampleIndex();
            if (index == -1)
                index = 0;
            index = sample + (index / 10) * 10;
            if (index < song.samples.size()) {
                selected.sample = song.samples[index];
            } else {
                sample = -1;
            }
        }
        if (mask) {
            editor->writeEvent(app->jamEvent(e, selected), selected, mask);
            return;
        }
    }

    switch (e.keysym.sym) {
    /* Sample select */
    case SDLK_KP_PLUS:
        if (!ctrl) {
            std::shared_lock songLock(song.mu);
            int index = editor->selectedSampleIndex();
            if (index == -1) {
                if (!song.samples.empty())
                    selected.sample = song.samples.front();
            } else if (index < song.samples.size() - 1) {
                selected.sample = song.samples[index + 1];
            }
        }
        break;
    case SDLK_KP_MINUS:
        if (!ctrl) {
            std::shared_lock songLock(song.mu);
            int index = editor->selectedSampleIndex();
            if (index > 0) {
                selected.sample = song.samples[index - 1];
            }
        }
        break;
    case SDLK_KP_MULTIPLY:
        {
            std::shared_lock songLock(song.mu);
            int index = editor->selectedSampleIndex();
            if (index != -1 && index < song.samples.size() - 10) {
                selected.sample = song.samples[index + 10];
            } else if (!song.samples.empty()) {
                selected.sample = song.samples.back();
            }
        }
        break;
    case SDLK_KP_DIVIDE:
        {
            std::shared_lock songLock(song.mu);
            int index = editor->selectedSampleIndex();
            if (index >= 10) {
                selected.sample = song.samples[index - 10];
            } else if (!song.samples.empty()) {
                selected.sample = song.samples.front();
            }
        }
        break;
    /* Pitch select */
    case SDLK_EQUALS:
        if (!ctrl && octave < 9) {
            octave++;
            if (selected.pitch != Event::NO_PITCH)
                selected.pitch += 12;
        }
        break;
    case SDLK_MINUS:
        if (!ctrl && octave > 0) {
            octave--;
            if (selected.pitch != Event::NO_PITCH)
                selected.pitch -= 12;
        }
        break;
    /* Special jam */
    case SDLK_BACKQUOTE:
        if (!e.repeat) {
            Event event = selected;
            event.special = Event::Special::FadeOut; // don't store
            editor->writeEvent(app->jamEvent(e, event), event, Event::SPECIAL);
        }
        break;
    case SDLK_1:
        if (!e.repeat) {
            Event event = selected;
            event.special = Event::Special::Slide;
            editor->writeEvent(app->jamEvent(e, event), event, Event::SPECIAL);
        }
        break;
    /* jam clear */
    case SDLK_KP_PERIOD:
        if (!e.repeat) {
            Event event = selected;
            event.sample.reset();
            editor->writeEvent(app->jamEvent(e, event), event, Event::SAMPLE);
        }
        break;
    case SDLK_BACKSLASH:
        if (!e.repeat) {
            Event event = selected;
            event.pitch = Event::NO_PITCH;
            editor->writeEvent(app->jamEvent(e, event), event, Event::PITCH);
        }
        break;
    case SDLK_QUOTE:
        if (!e.repeat) {
            Event event = selected;
            event.velocity = Event::NO_VELOCITY;
            editor->writeEvent(app->jamEvent(e, event), event, Event::VELOCITY);
        }
        break;
    case SDLK_KP_ENTER:
        if (!e.repeat) {
            // selected already shouldn't have special
            editor->writeEvent(app->jamEvent(e, selected),
                               selected, Event::SPECIAL);
        }
        break;
    }
}

void EventKeyboard::keyUp(const SDL_KeyboardEvent &e)
{
    int pitch = -1, sampleI = -1;
    if (!(e.keysym.mod & KMOD_CTRL)) {
        pitch = pitchKeymap(e.keysym.scancode);
        sampleI = sampleKeymap(e.keysym.scancode);
    }
    if (pitch < 0 && sampleI < 0) {
        switch (e.keysym.sym) {
            // not SDLK_BACKQUOTE (already FadeOut)
            case SDLK_1:
            case SDLK_KP_PERIOD:
            case SDLK_BACKSLASH:
            case SDLK_QUOTE:
            case SDLK_KP_ENTER:
                break; // good
            default:
                return;
        }
    }

    Event fadeEvent;
    fadeEvent.special = Event::Special::FadeOut;
    if (app->jamEvent(e, fadeEvent)) { // if playing
        editor->writeEvent(true, fadeEvent, Event::ALL);
    }
}

int EventKeyboard::pitchKeymap(SDL_Scancode key)
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

int EventKeyboard::sampleKeymap(SDL_Scancode key)
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
