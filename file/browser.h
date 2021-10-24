#pragma once
#include <common.h>

#include <filesystem>

namespace chromatracker::file {

class Browser
{
public:
    enum class FileType
    {
        Module, Sample
    };
    Browser(FileType type);

    void open(std::filesystem::path path);

    const std::filesystem::path path() const;
    const vector<std::filesystem::path> & directories() const;
    const vector<std::filesystem::path> & files() const;

private:
    bool extensionMatch(string ext);

    const FileType type;
    std::filesystem::path _path;
    // directories may actually be files (eg. module files contain samples)
    vector<std::filesystem::path> _directories;
    vector<std::filesystem::path> _files;
};

} // namespace
