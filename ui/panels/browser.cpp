#include "browser.h"
#include <app.h>
#include <ui/text.h>
#include <ui/theme.h>
#include <SDL2/SDL_filesystem.h>

namespace chromatracker::ui::panels {

const glm::vec4 C_DIRECTORY {0.8, 0.8, 0.8, 1};

Browser::Browser(const App *app, file::FileType type,
                 std::function<void(file::Path)> callback)
    : app(app)
    , type(type)
    , callback(callback)
{
    open(std::filesystem::current_path());
}

void Browser::open(file::Path path)
{
    this->path = path;
    file::listDirectory(path, type, directories, files);
    selected = 0;
}

void Browser::draw(Rect rect)
{
    app->scissorRect(rect);

    glm::vec2 textPos = rect(TL);
    textPos = drawText(path.string(), rect(TL), C_ACCENT_LIGHT)(BL);

    int i = 0;
    for (auto &directory : directories) {
        textPos = drawText(directory.filename().string(), textPos,
                           i == selected ? C_ACCENT_LIGHT : C_DIRECTORY)(BL);
        i++;
    }
    for (auto &file : files) {
        textPos = drawText(file.filename().string(), textPos,
                           i == selected ? C_ACCENT_LIGHT : C_WHITE)(BL);
        i++;
    }
}

void Browser::keyDown(const SDL_KeyboardEvent &e)
{
    switch (e.keysym.sym) {
    case SDLK_DOWN:
        selected++;
        break;
    case SDLK_UP:
        selected--;
        break;
    case SDLK_RETURN:
        if (selected >= 0 && selected < directories.size()) {
            open(directories[selected]);
        } else {
            int selectedFile = selected - directories.size();
            if (selectedFile >= 0 && selectedFile < files.size()) {
                auto &path = files[selectedFile];
                callback(path); // may destroy browser
                return;
            }
        }
        break;
    case SDLK_BACKSPACE:
        open(path.parent_path());
        break;
    case SDLK_ESCAPE:
        callback(file::Path()); // may destroy browser
        return;
    case SDLK_1:
        if (e.keysym.mod & KMOD_CTRL) {
            // bookmark for testing TODO remove
            open("D:\\Google Drive\\mods");
        }
        break;
    }
}


} // namespace
