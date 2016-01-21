#include <windows.h>

#include <lua/lua.hpp>

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

    //////////////////////////////////////////////////////////////////////////

    static int charset_a2u(lua_State* L) {
        auto a = lua_tostring(L, 1);
        auto u = a2u(a);
        lua_pushlstring(L, (const char*)u.c_str(), (u.size() + 1)*sizeof(u[0]));
        return 1;
    }

    static int charset_u2a(lua_State* L) {
        auto u = (const wchar_t*)lua_tostring(L, 1);
        auto a = u2a(u);
        lua_pushstring(L, a.c_str());
        return 1;
    }

    static int charset_e2u(lua_State* L) {
        auto e = lua_tostring(L, 1);
        auto u = e2u(e);
        lua_pushlstring(L, (const char*)u.c_str(), (u.size() + 1)*sizeof(u[0]));
        return 1;
    }

    static int charset_u2e(lua_State* L) {
        auto u = (const wchar_t*)lua_tostring(L, 1);
        auto e = u2e(u);
        lua_pushstring(L, e.c_str());
        return 1;
    }

    static int charset_a2e(lua_State* L) {
        auto a = lua_tostring(L, 1);
        auto e = a2e(a);
        lua_pushstring(L, e.c_str());
        return 1;
    }

    static int charset_e2a(lua_State* L) {
        auto e = lua_tostring(L, 1);
        auto a = e2a(e);
        lua_pushstring(L, a.c_str());
        return 1;
    }


    int luaopen_charset(lua_State* L) {
        static const luaL_Reg charsetlib[] = {
            { "a2u", charset_a2u },
            { "u2a", charset_u2a },
            { "e2u", charset_e2u },
            { "u2e", charset_u2e },
            { "a2e", charset_a2e },
            { "e2a", charset_e2a },
            { NULL, NULL }
        };

        luaL_newlib(L, charsetlib);

        return 1;
    }
}
