#pragma once

#include <windows.h>

namespace taoweb {
    class locker {
    public:
        locker() {
            ::InitializeCriticalSection(&_cs);
        }

        virtual ~locker() {
            ::DeleteCriticalSection(&_cs);
        }

        void lock() {
            return ::EnterCriticalSection(&_cs);
        }

        void unlock() {
            return ::LeaveCriticalSection(&_cs);
        }

        bool try_lock() {
            return !!::TryEnterCriticalSection(&_cs);
        }

    protected:
        CRITICAL_SECTION _cs;
    };

}
