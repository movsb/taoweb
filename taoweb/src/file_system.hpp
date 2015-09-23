#pragma once

#include <cstdlib>

#include <string>

#include <Shlwapi.h>

namespace taoweb {
    namespace file_system {
        enum class file_type {
            error = -1,
            file,
            directory,
            not_found,
            access_denied,
        };

        bool file_exists(const char* file) {
            return !!::PathFileExists(file);
        }

        std::string exe_dir() {
            char path[MAX_PATH];
            ::GetModuleFileName(nullptr, path, sizeof(path));
            *strrchr(path, '\\') = '\0';
            
            return path;
        }

        std::string cur_dir() {
            return R"(C:\Users\Tao\Desktop\wwwroot)";
        }

        file_type file_attr(const char* file) {
            DWORD dw_attr = ::GetFileAttributes(file);
            if (dw_attr == INVALID_FILE_ATTRIBUTES) {
                return file_type::error;
            }

            if (dw_attr & FILE_ATTRIBUTE_DIRECTORY)
                return file_type::directory;

            if (dw_attr & FILE_ATTRIBUTE_NORMAL || dw_attr & FILE_ATTRIBUTE_ARCHIVE)
                return file_type::file;

            return file_type::error;
        }

        std::string simplify_path(const char* path) {
            char new_path[1024];
            char* i = new_path;
            const char* p = path;

            ::strncpy(new_path, path, sizeof(new_path));

            char c;
            while (c = *p++) {
                if (c == '/') {
                    while (i > new_path && i[-1] == '/')
                        i--;

                    *i++ = '/';
                    continue;
                }
                else if (c == '.' && i[-1] == '/') {
                    c = *p++;
                    if (c == '/') {
                        continue;
                    }
                    else if (c == '.') {
                        c = *p++;
                        if (c == '/' || c == '\0') {
                            if (i - new_path > 1) i--;
                            while (i > new_path && i[-1] != '/')
                                i--;

                            if (c == '\0') break;
                            else continue;
                        }
                        else {
                            *i++ = '.';
                            *i++ = '.';
                            *i++ = c;
                            continue;
                        }
                    }
                    else if (c == '\0') {
                        break;
                    }
                    else {
                        *i++ = '.';
                        *i++ = c;
                        continue;
                    }
                }
                else {
                    *i++ = c;
                }
            }

            if (i == new_path) *i++ = '/';
            if (i - new_path > 1 && i[-1] == '/') i--;

            *i++ = '\0';

            std::string np(new_path);
            return np;
        }

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
    }
}
