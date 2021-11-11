#include "types.h"
#include "chromaloader.h"
#include "itloader.h"
#include <algorithm>
#include <cctype>
#include <exception>
#include <iomanip>
#include <sstream>

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
    if (ext == ".it" || ext == ".chroma")
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
    } else if (ext == ".chroma") {
        return new chroma::Loader(stream);
    } else {
        SDL_RWclose(stream);
        return nullptr;
    }
}

SampleLoader * sampleLoaderForPath(Path path)
{
    Path parent = path.parent_path();
    string ext = normalizedExtension(path);
    string parentExt = normalizedExtension(parent);
    if (parentExt == ".it") {
        Path modulePath = path.parent_path();
        SDL_RWops *stream = SDL_RWFromFile(modulePath.string().c_str(), "r");
        if (!stream) {
            cout << "Error opening stream: " <<SDL_GetError()<< "\n";
            return nullptr;
        }
        int index = std::stoi(path.filename()) - 1; // parse the first number
        return new ModuleSampleLoader(new ITLoader(stream), index);
    } else {
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
        for (int i = 0; i < sampleNames.size(); i++) {
            std::ostringstream nameStream;
            nameStream << std::setw(2) << std::setfill('0') << (i + 1) << " "
                << sampleNames[i];
            files.push_back(path / nameStream.str());
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

ModuleSampleLoader::ModuleSampleLoader(ModuleLoader *mod, int index)
    : mod(mod)
    , index(index)
{}

void ModuleSampleLoader::loadSample(shared_ptr<Sample> sample)
{
    mod->loadSample(index, sample);
}

} // namespace
