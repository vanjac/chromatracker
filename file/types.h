#pragma once
#include <common.h>

#include <song.h>
#include <filesystem>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

enum class FileType
{
    Unknown, Module, Sample
};

class ModuleLoader
{
public:
    virtual ~ModuleLoader() = default;
    virtual void loadSong(Song *song) = 0;
};

FileType typeForPath(std::filesystem::path path);
// constructs loader, caller should take ownership
ModuleLoader * moduleLoaderForPath(std::filesystem::path path,
                                   SDL_RWops *stream);

} // namespace
