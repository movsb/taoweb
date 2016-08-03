#pragma once

#include <cstdio>
#include <cstdarg>

#include <mutex>

namespace taoweb {

class Logger
{
public:
    Logger() {
    }

    ~Logger() {

    }

public:
    int info(const char* fmt, ...) {
        return _log(stdout, fmt);
    }

    int err(const char* fmt, ...) {
        return _log(stderr, fmt);
    }

private:
    int _log(FILE* fp, const char*& fmt, ...) {
        int r;
        va_list args;
        va_start(args, fmt);

        _mtx.lock();

        r = vfprintf(fp, fmt, args);

        _mtx.unlock();

        va_end(args);
        return r;
    }

private:
    std::mutex _mtx;
};

}

extern taoweb::Logger g_logger;
