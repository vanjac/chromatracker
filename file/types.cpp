#include "itloader.h"
#include "types.h"
#include <algorithm>
#include <cctype>

namespace chromatracker::file {

string normalizedExtension(Path path)
{
    string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){ return std::tolower(c); }); // :(
    return ext;
}

FileType typeForPath(Path path)
{
    string ext = normalizedExtension(path);
    if (ext == ".it")
        return FileType::Module;
    else if (ext == ".wav" || ext == ".iti")
        return FileType::Sample;
    else
        return FileType::Unknown;
}

ModuleLoader * moduleLoaderForPath(Path path, SDL_RWops *stream)
{
    string ext = normalizedExtension(path);
    if (ext == ".it")
        return new ITLoader(stream);
    else
        return nullptr;
}

void listDirectory(Path path, FileType type,
                   vector<Path> &directories, vector<Path> &files)
{
    directories.clear();
    files.clear();
    for (const auto &dir : std::filesystem::directory_iterator(path)) {
        auto &childPath = dir.path();
        if (dir.is_directory()) {
            directories.push_back(childPath);
        } else if (typeForPath(childPath) == type) {
            files.push_back(childPath);
        }
    }
}

} // namespace
