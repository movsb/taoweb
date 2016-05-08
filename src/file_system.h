#pragma once

#include <cstdlib>
#include <cstdint>

#include <string>
#include <vector>
#include <tuple>
#include <functional>

#include <Shlwapi.h>

namespace file_system {
    std::tuple<std::string, std::string> base_and_ext(const std::string& path);

    bool exists(const char* file);
    std::string exe_dir();

    enum class file_type {
        error = -1,
        file,
        directory,
        not_found,
        access_denied,
    };

    file_type type(const char* file);

    struct file_entry {
        bool        isdir;
        std::string file;
    };

    void get_directory_files(const char* base, std::vector<file_entry>* files);

    struct stat_t {
        uint32_t    attr;
        FILETIME    creation_time;
        FILETIME    access_time;
        FILETIME    write_time;
        uint64_t    size;
        uint64_t    inode;
    };

    class file_object_t {
    public:
        file_object_t(const char* file)
            :_file(file)
        {}

        ~file_object_t(){
            close();
        }

    public:
        bool open();

        bool close();

        void read_block(int size, std::function<void(const void* buf, int size)> callback);

        stat_t* stat();

        std::string etag();

        uint64_t size();

    protected:
        stat_t          _stat;
        std::string     _file;
        HANDLE          _handle;
    };
}
