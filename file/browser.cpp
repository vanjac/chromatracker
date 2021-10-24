#include "browser.h"
#include <algorithm>
#include <cctype>
#include <SDL2/SDL_filesystem.h>

namespace fs = std::filesystem;

namespace chromatracker::file {

Browser::Browser(FileType type)
    : type(type)
{
    open(fs::current_path());
}

void Browser::open(fs::path path)
{
    _path = path;
    _directories.clear();
    _files.clear();
    for (const auto &dir : fs::directory_iterator(_path)) {
        auto &childPath = dir.path();
        if (dir.is_directory()) {
            _directories.push_back(childPath);
        } else if (extensionMatch(childPath.extension().string())) {
            _files.push_back(childPath);
        }
    }
}

const fs::path Browser::path() const
{
    return _path;
}

const vector<fs::path> & Browser::directories() const
{
    return _directories;
}

const vector<fs::path> & Browser::files() const
{
    return _files;
}

bool Browser::extensionMatch(string ext) {
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){ return std::tolower(c); }); // :(
    switch (type) {
    case FileType::Module:
        return ext == ".it";
    case FileType::Sample:
        return ext == ".wav" || ext == ".iti";
    default:
        return true;
    }
}

} // namespace
