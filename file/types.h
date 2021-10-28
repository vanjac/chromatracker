#pragma once
#include <common.h>

#include <song.h>
#include <filesystem>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

using Path = std::filesystem::path;

enum class FileType
{
    Unknown, Module, Sample
};

class ModuleLoader
{
public:
    virtual ~ModuleLoader() = default;
    virtual void loadSong(Song *song) = 0; // song should be cleared
    virtual vector<string> listSamples() = 0;
    virtual void loadSample(int index, shared_ptr<Sample> sample) = 0;
};

FileType typeForPath(Path path);
// constructs loader, caller should take ownership
ModuleLoader * moduleLoaderForPath(Path path, SDL_RWops *stream);

// for Sample type, Module files are treated as directories of samples
void listDirectory(Path path, FileType type,
                   vector<Path> &directories, vector<Path> &files);

} // namespace
