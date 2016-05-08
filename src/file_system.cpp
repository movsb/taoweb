#include <memory>
#include <regex>

#include "charset.h"

#include "file_system.h"

namespace file_system {

    std::tuple<std::string, std::string> base_and_ext(const std::string& path) {
        std::smatch results;
        std::regex_match(path, results, std::regex(R"((.+?)(\.[^.]+)?)"));
        return std::make_tuple(results[1], results[2]);
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
        if(!::PathFileExists(file))
            return file_type::not_found;

        DWORD dw_attr = ::GetFileAttributes(file);
        if (dw_attr == INVALID_FILE_ATTRIBUTES)
            return file_type::error;

        if (dw_attr & FILE_ATTRIBUTE_DIRECTORY)
            return file_type::directory;

        return file_type::file;
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

    // ------------------------------------------------------------------------------------------------------------------

    uint64_t file_object_t::size() {
        if(auto st = stat()) {
            return st->size;
        } else {
            return 0;
        }
    }

    std::string file_object_t::etag() {
        if(auto st = stat()) {
            char tagstr[19];
            ::sprintf(tagstr, R"("%08X%08X")", st->inode); // TODO
            return tagstr;
        } else {
            return "";
        }
    }

    stat_t* file_object_t::stat() {
        BY_HANDLE_FILE_INFORMATION info;
        if(!::GetFileInformationByHandle(_handle, &info))
            return nullptr;

        stat_t* st = &_stat;

        st->attr = info.dwFileAttributes;
        st->size = ((uint64_t)info.nFileSizeHigh << 32) + info.nFileSizeLow;
        st->inode = ((uint64_t)info.nFileIndexHigh << 32) + info.nFileIndexLow;
        st->creation_time = info.ftCreationTime;
        st->write_time = info.ftLastWriteTime;
        st->access_time = info.ftLastAccessTime;

        return st;
    }

    bool file_object_t::close() {
        if(!_handle || ::CloseHandle(_handle)) {
            _handle = nullptr;
            return true;
        }

        return false;
    }

    bool file_object_t::open() {
        _handle = ::CreateFile(_file.c_str(),
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(_handle == INVALID_HANDLE_VALUE)
            _handle = nullptr;

        return !!_handle;
    }

    void file_object_t::read_block(int size, std::function<void(const void* buf, int size)> callback) {
        DWORD dwRead;
        std::unique_ptr<unsigned char> block(new unsigned char[size]);

        while(::ReadFile(_handle, block.get(), size, &dwRead, nullptr) && dwRead > 0) {
            callback(block.get(), (int)dwRead);
        }
    }

}
