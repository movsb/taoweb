#include <windows.h>

#include "charset.h"

namespace charset {
	std::wstring __ae2u(UINT cp, const std::string& src) {
		if (src.size() > 0) {
			int cch = ::MultiByteToWideChar(cp, 0, src.c_str(), -1, NULL, 0);
			if (cch > 0) {
				wchar_t* ws = new wchar_t[cch];
				if (::MultiByteToWideChar(cp, 0, src.c_str(), -1, ws, cch) > 0) {
					std::wstring r(ws);
					delete[] ws;
					return r;
				}
				else {
					delete[] ws;
				}
			}
		}
		return std::wstring();
	}

	std::string __u2ae(UINT cp, const std::wstring& src) {
		if (src.size() > 0) {
			int cb = ::WideCharToMultiByte(cp, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
			if (cb > 0) {
				char* ms = new char[cb];
				if (::WideCharToMultiByte(cp, 0, src.c_str(), -1, ms, cb, NULL, NULL) > 0) {
					std::string r(ms);
					delete[] ms;
					return r;
				}
				else {
					delete[] ms;
				}
			}
		}
		return std::string();
	}
}
