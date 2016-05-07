#pragma once

#include <string>

#include <windows.h>

/*
	https://github.com/movsb/taoexec/blob/master/src/charset.h

    A - ANSI(extended)
    U - UNICODE(16)
    E - UTF-8
    T - typedef-ed
*/

namespace charset {
	std::wstring __ae2u(UINT cp, const std::string& src);
	std::string  __u2ae(UINT cp, const std::wstring& src);

    // from ANSI(extended) to UNICODE
	inline std::wstring a2u(const std::string& src) {
		return __ae2u(CP_ACP, src);
	}
    // from UNICODE to ANSI(extended)
	inline std::string u2a(const std::wstring& src) {
		return __u2ae(CP_ACP, src);
	}
    // from UTF-8 to UNICODE
	inline std::wstring e2u(const std::string& src) {
		return __ae2u(CP_UTF8, src);
	}
    // from UNICODE to UTF-8
	inline std::string u2e(const std::wstring& src) {
		return __u2ae(CP_UTF8, src);
	}
    // from ANSI(extended) to UTF-8
	inline std::string a2e(const std::string& src) {
		return u2e(a2u(src));
	}
    // from UTF-8 to ANSI(extended)
	inline std::string e2a(const std::string& src) {
		return u2a(e2u(src));
	}
    // from UTF-8 to ANSI or UNICODE
#if defined(_UNICODE) || defined(UNICODE)
	inline std::wstring e2t(const std::string& src) {
		return e2u(src);
	}
#else
	inline std::string  e2t(const std::string& src) {
		return e2a(src);
	}
#endif

}
