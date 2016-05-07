#include <memory>

#include <windows.h>

#include "charset.h"

namespace charset {
	std::wstring __ae2u(UINT cp, const std::string& src) {
        std::wstring r;
		if (src.size() > 0) {
			int cch = ::MultiByteToWideChar(cp, 0, src.c_str(), -1, NULL, 0);
			if (cch > 0) {
                std::unique_ptr<wchar_t> ws(new wchar_t[cch]);
				if (::MultiByteToWideChar(cp, 0, src.c_str(), -1, ws.get(), cch) > 0) {
                    r.assign(ws.get());
				}
			}
		}
		return r;
	}

	std::string __u2ae(UINT cp, const std::wstring& src) {
        std::string r;
		if (src.size() > 0) {
			int cb = ::WideCharToMultiByte(cp, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
			if (cb > 0) {
                std::unique_ptr<char> ms(new char[cb]);
				if (::WideCharToMultiByte(cp, 0, src.c_str(), -1, ms.get(), cb, NULL, NULL) > 0) {
					r.assign(ms.get());
				}
			}
		}
		return r;
	}
}
