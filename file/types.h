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

// any methods may throw exceptions
class ModuleLoader
{
public:
    virtual ~ModuleLoader() = default;
    virtual void loadSong(Song *song) = 0; // song should be cleared
    virtual vector<string> listSamples() = 0;
    virtual void loadSample(int index, shared_ptr<Sample> sample) = 0;
};

// any methods may throw exceptions
class SampleLoader
{
public:
    virtual ~SampleLoader() = default;
    virtual void loadSample(shared_ptr<Sample> sample) = 0;
};

class ModuleSampleLoader : public SampleLoader
{
public:
    ModuleSampleLoader(ModuleLoader *mod, int index); // takes ownership
    void loadSample(shared_ptr<Sample> sample) override;
private:
    unique_ptr<ModuleLoader> mod;
    int index;
};

FileType typeForPath(Path path);
// constructs loader, caller should take ownership (could return null!)
ModuleLoader * moduleLoaderForPath(Path path);
SampleLoader * sampleLoaderForPath(Path path);

// for Sample type, Module files are treated as directories of samples
void listDirectory(Path path, FileType type,
                   vector<Path> &directories, vector<Path> &files);

} // namespace
