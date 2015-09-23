#pragma once

#include <cstring>
#include <ctime>

#include <string>
#include <sstream>

#include <WinSock2.h>
#include <windows.h>

#include "file_system.hpp"

namespace taoweb {
    namespace http {
        std::string gmtime() {
            time_t now = time(nullptr);
            tm* gmt = ::gmtime(&now);

            // http://en.cppreference.com/w/c/chrono/strftime
            // e.g.: Sat, 22 Aug 2015 11:48:50 GMT
            //       5+   3+4+   5+   9+       3   = 29
            const char* fmt = "%a, %d %b %Y %H:%M:%S GMT";
            char tstr[30];

            strftime(tstr, sizeof(tstr), fmt, gmt);

            return tstr;
        }

        class error_page_t {
        protected:
            struct one_page_t {
                int code;
                int header_len;
                int html_len;
                const char* header;
                const char* html;
            };
        public:
            error_page_t() {
                static one_page_t pages[] = {
                    { 200, 0, 0, "HTTP/1.1 200 OK", "" },
                    { 304, 0, 0, "HTTP/1.1 304 Not Modified", "" },
                    { 400, 0, 0, "HTTP/1.1 400 Bad Request", "<html>\n<head>\n<title>400 Bad Request</title>\n</head>\n<body><center><h1>400 Bad Request</h1><hr />taoweb/0.0</center></body>\n</html>\n" },
                    { 403, 0, 0, "HTTP/1.1 403 Forbidden", "<html>\n<head>\n<title>403 Forbidden</title>\n</head>\n<body><center><h1>403 Forbidden</h1><hr />taoweb/0.0</center></body>\n</html>\n" },
                    { 404, 0, 0, "HTTP/1.1 404 Not Found", "<html>\n<head>\n<title>404 Not Found</title>\n</head>\n<body><center><h1>404 Not Found</h1><hr />taoweb/0.0</center></body>\n</html>\n" },
                    { 500, 0, 0, "HTTP/1.1 500 Internal Server Error", "<html>\n<head>\n<title>500 Internal Server Error</title>\n</head>\n<body><center><h1>500 Internal Server Error</h1><hr />taoweb/0.0</center></body>\n</html>\n" },
                };

                int count = _countof(pages);
                for (int i = 0; i < count; i++) {
                    pages[i].header_len = (int)::strlen(pages[i].header);
                    pages[i].html_len = (int)::strlen(pages[i].html);
                }

                _pages = &pages[0];
                _count = count;
            }

            const one_page_t& operator[](int code) {
                int i;
                for (i = 0; i < _count; i++) {
                    if (_pages[i].code == code)
                        break;
                }

                if (i >= _count) i = _count - 1; // 500

                return _pages[i];
            }

            void output(SOCKET& fd, int code) {
                auto& page = (*this)[code];

                std::stringstream ss;
                ss << page.header << "\r\n"
                    << "Server: taoweb/0.0\r\n"
                    << "Connection: close\r\n"
                    << "Date: " << gmtime() << "\r\n"
                    << "\r\n";

                auto& header_str = ss.str();
                ::send(fd, header_str.c_str(), header_str.size(), 0);

                ::send(fd, page.html, page.html_len, 0);
            }
        protected:
            const one_page_t*   _pages;
            int                 _count;
        };

        /*
        A-Z a-z 0-9
        - . _ ~ : / ? # [ ] @ ! $ & ' ( ) * + , ; =
        no space
        */
        bool decode_uri(const std::string& uri, std::string* result) {
            if (uri.size() == 0 || !result) return false;

            auto p = uri.c_str();
            auto q = p + uri.size();

            for (; p < q;) {
                auto c = *p;
                if (c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || strchr("-._~:/?#[]@!$&'()*+,;=", c)) {
                    *result += c;
                    p++;
                }
                else if (c == '%') {
                    if (p + 1 < q && p[1] == '%') {
                        *result += '%';
                        p++;
                    }

                    if (p + 2 < q) {
                        unsigned char v = 0;
                        p++;
                        if (*p >= '0' && *p <= '9') v = *p - '0';
                        else if (*p >= 'A' && *p <= 'F') v = *p - 'A' + 10;
                        else return false;
                        p++;
                        v = v * 16;
                        if (*p >= '0' && *p <= '9') v += *p - '0';
                        else if (*p >= 'A' && *p <= 'F') v += *p - 'A' + 10;
                        else return false;

                        p++;
                        *result += v;
                    }
                    else {
                        return false;
                    }
                }
                else {
                    return false;
                }
            }
            return true;
        }

        std::string mime(const std::string& file) {
            static struct {
                const char* mime;
                const char* exts;
            } known_mimes[] = {
                {"text/html",           "html\x00htm\x00shtml\x00"},
                {"text/css",            "css\x00"},
                {"text/xml",            "xml\x00"},
                {"text/javascript",     "js\x00"},

                {"text/plain",          "txt\x00ini\x00"},
                {"text/plain",          "c\x00cc\x00\x00cpp\x00cxx\x00h\x00hpp\x00"},
                {"text/plain",          "php\x00sh\x00"},

                {"image/gif",           "gif\x00"},
                {"image/jpeg",          "jpg\x00jpeg\x00"},
                {"image/png",           "png\x00"},
                {"image/x-icon",        "ico\x00"},
                {"image/x-ms-bmp",      "bmp\x00"},

                {"application/json",    "json\x00"},
            };

            const char* octet_stream = "application/octet-stream";
            const int count = sizeof(known_mimes) / sizeof(known_mimes[0]);

            auto ext = taoweb::file_system::ext(file);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if(ext.size()) {
                auto ad = ext.c_str() + 1; // after dot
                for(int i = 0; i < count; i++) {
                    const char* p = known_mimes[i].exts;
                    while(*p != '\0') {
                        if(strcmp(ad, p) == 0) // found!
                            return known_mimes[i].mime;
                        while(*p++) // skip to next
                            ;
                    }
                }
            }

            return octet_stream;
        }

    }
}
