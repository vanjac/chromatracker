#pragma once
#include <common.h>

#include <file/types.h>
#include <ui/ui.h>
#include <filesystem>
#include <functional>
#include <SDL2/SDL_events.h>

namespace chromatracker { class App; }

namespace chromatracker::ui::panels {

class Browser
{
public:
    Browser(App *app, file::FileType type,
            std::function<void(file::Path)> callback);
    ~Browser();

    void draw(Rect rect);
    void keyDown(const SDL_KeyboardEvent &e);

private:
    void open(file::Path path);

    App * const app;
    const file::FileType type;
    const std::function<void(file::Path)> callback;

    file::Path path;
    vector<file::Path> directories;
    vector<file::Path> files;

    int selected {0};
};

} // namespace
