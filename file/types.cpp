#include "itloader.h"
#include "types.h"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace chromatracker::file {

string normalizedExtension(fs::path path)
{
    string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){ return std::tolower(c); }); // :(
    return ext;
}

FileType typeForPath(fs::path path)
{
    string ext = normalizedExtension(path);
    if (ext == ".it")
        return FileType::Module;
    else if (ext == ".wav" || ext == ".iti")
        return FileType::Sample;
    else
        return FileType::Unknown;
}

ModuleLoader * moduleLoaderForPath(fs::path path, SDL_RWops *stream)
{
    string ext = normalizedExtension(path);
    if (ext == ".it")
        return new ITLoader(stream);
    else
        return nullptr;
}

} // namespace
