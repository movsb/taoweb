#pragma once

#include <cstdlib>
#include <cstdint>

#include <string>
#include <vector>

#include <Shlwapi.h>

namespace file_system {
    std::string ext(const std::string& file);
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
    std::string simplify_path(const char* path);

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
        bool open() {
            _handle = ::CreateFile(_file.c_str(),
                GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (_handle == INVALID_HANDLE_VALUE)
                _handle = nullptr;

            return !!_handle;
        }

        bool close() {
            if (!_handle || ::CloseHandle(_handle)) {
                _handle = nullptr;
                return true;
            }

            return false;
        }

        bool read(uint8_t* buf, uint32_t size, uint32_t* n) {
            return !!::ReadFile(_handle, buf, size, LPDWORD(n), nullptr);
        }

        stat_t* stat() {
            BY_HANDLE_FILE_INFORMATION info;
            if (!::GetFileInformationByHandle(_handle, &info))
                return nullptr;

            stat_t* st = &_stat;

            st->attr            = info.dwFileAttributes;
            st->size            = ((uint64_t)info.nFileSizeHigh << 32) + info.nFileSizeLow;
            st->inode           = ((uint64_t)info.nFileIndexHigh << 32) + info.nFileIndexLow;
            st->creation_time   = info.ftCreationTime;
            st->write_time      = info.ftLastWriteTime;
            st->access_time     = info.ftLastAccessTime;

            return st;
        }

        std::string etag() {
            if (auto st = stat()){
                char tagstr[19];
                ::sprintf(tagstr, R"("%08X%08X")", st->inode); // TODO
                return tagstr;
            }
            else {
                return "";
            }
        }

        uint64_t size() {
            if (auto st = stat()){
                return st->size;
            }
            else {
                return 0;
            }
        }

    protected:
        stat_t          _stat;
        std::string     _file;
        HANDLE          _handle;
    };

    //////////////////////////////////////////////////////////////////////////

    int luaopen_filesystem(lua_State* L);
}
