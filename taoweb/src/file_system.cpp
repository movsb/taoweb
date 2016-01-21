#include <lua/lua.hpp>

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

    //////////////////////////////////////////////////////////////////////////

    static int filesystem_ext(lua_State* L) {
        auto en = luaL_checkstring(L, 1);
        auto fe = ext(en);
        lua_pushstring(L, fe.c_str());
        return 1;
    }

    static int filesystem_exists(lua_State* L) {
        auto en = luaL_checkstring(L, 1);
        auto an = charset::e2a(en);
        lua_pushboolean(L, exists(an.c_str()));
        return 1;
    }

    static int filesystem_exe_dir(lua_State* L) {
        auto dir = exe_dir();
        lua_pushstring(L, charset::a2e(dir).c_str());
        return 1;
    }

    static int filesystem_type(lua_State* L) {
        auto f = luaL_checkstring(L, 1);
        file_type t = type(charset::e2a(f).c_str());
        const char* ts;
        switch (t) {
        case file_type::file:
            ts = "file";
            break;
        case file_type::directory:
            ts = "dir";
            break;
        default:
            ts = nullptr;
            break;
        }

        if (ts) lua_pushstring(L, ts);
        else lua_pushnil(L);

        return 1;
    }

    static int filesystem_simplify_path(lua_State* L) {
        auto f = luaL_checkstring(L, 1);
        lua_pushstring(L, simplify_path(f).c_str());
        return 1;
    }

    static int filesystem_list(lua_State* L) {
        auto d = luaL_checkstring(L, 1);
        auto a = charset::e2a(d);
        auto t = type(a.c_str());
        if (t != file_type::directory) {
            lua_pushnil(L);
            return 1;
        }

        std::vector<file_entry> files;
        get_directory_files(a.c_str(), &files);

        lua_createtable(L, files.size(), 0);

        int i = 1;
        for (auto& f : files) {
            lua_createtable(L, 0, 2);

            lua_pushboolean(L, f.isdir);
            lua_setfield(L, -2, "isdir");

            lua_pushstring(L, charset::a2e(f.file).c_str());
            lua_setfield(L, -2, "file");

            lua_seti(L, -2, i++);
        }

        return 1;
    }

    //////////////////////////////////////////////////////////////////////////
    int luaopen_filesystem(lua_State* L) {
        static const luaL_Reg filesystemlib[] = {
            { "ext",        filesystem_ext},
            { "exists",     filesystem_exists},
            { "exe_dir",    filesystem_exe_dir},
            { "type",       filesystem_type},
            { "simplify",   filesystem_simplify_path},
            { "list",       filesystem_list},
            {nullptr, nullptr}
        };

        luaL_newlib(L, filesystemlib);

        return 1;
    }

}
