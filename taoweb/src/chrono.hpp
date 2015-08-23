#pragma once

#include <ctime>
#include <string>

namespace taoweb {
    std::string http_gmtime() {
        time_t now = time(nullptr);
        tm* gmt = gmtime(&now);

        // http://en.cppreference.com/w/c/chrono/strftime
        // e.g.: Sat, 22 Aug 2015 11:48:50 GMT
        //       5+   3+4+   5+   9+       3   = 29
        const char* fmt = "%a, %d %b %Y %H:%M:%S GMT";
        char tstr[30];

        strftime(tstr, sizeof(tstr), fmt, gmt);

        return tstr;
    }
}
