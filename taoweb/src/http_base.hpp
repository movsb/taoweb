#pragma once

#include <cstring>

#include <string>
#include <sstream>

#include <WinSock2.h>
#include <windows.h>

#include "chrono.hpp"

namespace taoweb {
    namespace http {
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
                    << "Date: " << taoweb::http_gmtime() << "\r\n"
                    << "\r\n";

                auto& header_str = ss.str();
                ::send(fd, header_str.c_str(), header_str.size(), 0);

                ::send(fd, page.html, page.html_len, 0);
            }
        protected:
            const one_page_t*   _pages;
            int                 _count;
        };
    }
}
