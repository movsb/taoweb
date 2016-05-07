#include <memory>

#include "charset.h"

#include "file_system.h"

namespace file_system {
    std::string ext(const std::string& file) {
        // 文件如果有扩展名，其小数点的两边必须有其它字符才算
        // file.exe 算，file.ext. 不算，.git 不算

        auto fs = file.find_last_of('/');
        auto bs = file.find_last_of('\\');
        auto d = file.find_last_of('.');

        auto sep = std::string::npos;
        if (fs != std::string::npos)
            sep = fs;
        if (bs != std::string::npos && bs > fs)
            sep = bs;

        if (d != std::string::npos // 有小数点
            && d != file.size() - 1 // 不是最后一个字符
            && (sep == std::string::npos || d > sep + 1) // 如果存在分隔符，那么一定不是分隔符后面的第1个字符
            ) {
            return file.c_str() + d;
        }
        return "";
    }

    bool exists(const char* file) {
        return !!::PathFileExists(file);
    }

    std::string exe_dir() {
        char path[MAX_PATH];
        ::GetModuleFileName(nullptr, path, sizeof(path));
        *strrchr(path, '\\') = '\0';
        return path;
    }

    file_type type(const char* file) {
        DWORD dw_attr = ::GetFileAttributes(file);
        if (dw_attr == INVALID_FILE_ATTRIBUTES)
            return file_type::error;

        if (dw_attr & FILE_ATTRIBUTE_DIRECTORY)
            return file_type::directory;

        return file_type::file;
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

    void get_directory_files(const char* base, std::vector<file_entry>* files) {
        std::vector<file_entry>& matches = *files;
        std::string folder(base);
        if (folder.back() != '/' && folder.back() != '\\')
            folder.append(1, '\\');

        WIN32_FIND_DATA wfd;
        std::string pattern = folder + "*.*";
        auto is_64bit = []() {
            BOOL b64;
            return IsWow64Process(GetCurrentProcess(), &b64)
                && b64 != FALSE;
        };
        if (is_64bit()) ::Wow64DisableWow64FsRedirection(nullptr);
        HANDLE hfind = ::FindFirstFile(pattern.c_str(), &wfd);
        if (hfind != INVALID_HANDLE_VALUE) {
            do {
                std::string file = wfd.cFileName;
                bool isdir = !!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                matches.push_back({ isdir, std::move(file) });
            } while (::FindNextFile(hfind, &wfd));
            ::FindClose(hfind);
        }
        if (is_64bit()) ::Wow64EnableWow64FsRedirection(TRUE);
    }

}
