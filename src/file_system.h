#pragma once

#include <cstdlib>
#include <cstdint>

#include <string>
#include <vector>
#include <tuple>
#include <functional>

#include <Shlwapi.h>

#include "logger.hpp"

namespace taoweb {

namespace file_system {
    std::tuple<std::string, std::string> base_and_ext(const std::string& path);

    bool exists(const char* file);
    std::string exe_dir();

    enum class FileType {
        error = -1,
        file,
        directory,
        not_found,
        access_denied,
    };

    FileType type(const char* file);

    struct FileEntry {
        bool        isdir;
        std::string file;
    };

    void get_directory_files(const char* base, std::vector<FileEntry>* files);

    struct Stat {
        uint32_t    attr;
        FILETIME    creation_time;
        FILETIME    access_time;
        FILETIME    write_time;
        uint64_t    size;
        uint64_t    inode;
    };

    class FileObject {
    public:
        FileObject(const char* file)
            :_file(file)
        {}

        ~FileObject(){
            close();
        }

    public:
        bool open();

        bool close();

        void read_block(int size, std::function<bool(const void* buf, int size)> callback);

        Stat* stat();

        std::string etag();

        uint64_t size();

    protected:
        Stat            _stat;
        std::string     _file;
        HANDLE          _handle;
    };
}

}
