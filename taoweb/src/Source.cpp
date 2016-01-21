#include <lua/lua.hpp>

#include "charset.h"
#include "file_system.h"

static void openlibs(lua_State* L) {
    static const luaL_Reg loadedlibs[] = {
        { "charset",    charset::luaopen_charset },
        { "filesystem", file_system::luaopen_filesystem },
    };

    for (auto& lib : loadedlibs) {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
}

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    openlibs(L);

    luaL_dofile(L, "../onload.lua");

    lua_close(L);
	return 0;
}
