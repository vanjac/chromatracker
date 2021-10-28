#include "types.h"
#include "itloader.h"
#include <algorithm>
#include <exception>
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

ModuleLoader * moduleLoaderForPath(Path path)
{
    SDL_RWops *stream = SDL_RWFromFile(path.string().c_str(), "r");
    if (!stream) {
        cout << "Error opening stream: " <<SDL_GetError()<< "\n";
        return nullptr;
    }
    string ext = normalizedExtension(path);
    if (ext == ".it") {
        return new ITLoader(stream);
    } else {
        SDL_RWclose(stream);
        return nullptr;
    }
}

void listDirectory(Path path, FileType type,
                   vector<Path> &directories, vector<Path> &files)
{
    directories.clear();
    files.clear();

    FileType pathType = typeForPath(path);
    if (type == FileType::Sample && pathType == FileType::Module) {
        unique_ptr<ModuleLoader> loader(moduleLoaderForPath(path));
        if (!loader)
            return;
        vector<string> sampleNames;
        try {
            sampleNames = loader->listSamples();
        } catch (std::exception e) {
            cout << "Error reading module: " <<e.what()<< "\n";
            return;
        }
        for (auto &name : sampleNames) {
            files.push_back(path / name);
        }
    } else {
        for (const auto &dir : std::filesystem::directory_iterator(path)) {
            auto &childPath = dir.path();
            FileType childType = typeForPath(childPath);
            if (childType == type) {
                files.push_back(childPath);
            } else if (dir.is_directory() || (type == FileType::Sample &&
                                              childType == FileType::Module)) {
                directories.push_back(childPath);
            }
        }
    }
}

} // namespace
