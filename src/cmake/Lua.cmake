# Defines a static `lua_static` target by fetching the official Lua 5.5
# source release. Idempotent.
if(TARGET lua_static)
    return()
endif()

include(FetchCache)
include(FetchContent)

FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    TLS_VERIFY OFF
)
FetchContent_MakeAvailable(lua)

# The Lua tarball ships only Makefiles, so we build a static library here.
# NOTE: the repo path may contain [ ] (e.g. D:/[DSH]/...), which CMake treats as
# glob character classes. Escape them so the glob still matches literal dirs.
# NOTE: paths here may contain [ ] (D:/[DSH]/...), which CMake globs treat as
# character classes, so an explicit source list is used instead of file(GLOB).
set(LUA_CORE_SOURCES
    "${lua_SOURCE_DIR}/src/lapi.c"
    "${lua_SOURCE_DIR}/src/lauxlib.c"
    "${lua_SOURCE_DIR}/src/lbaselib.c"
    "${lua_SOURCE_DIR}/src/lcode.c"
    "${lua_SOURCE_DIR}/src/lcorolib.c"
    "${lua_SOURCE_DIR}/src/lctype.c"
    "${lua_SOURCE_DIR}/src/ldblib.c"
    "${lua_SOURCE_DIR}/src/ldebug.c"
    "${lua_SOURCE_DIR}/src/ldo.c"
    "${lua_SOURCE_DIR}/src/ldump.c"
    "${lua_SOURCE_DIR}/src/lfunc.c"
    "${lua_SOURCE_DIR}/src/lgc.c"
    "${lua_SOURCE_DIR}/src/linit.c"
    "${lua_SOURCE_DIR}/src/liolib.c"
    "${lua_SOURCE_DIR}/src/llex.c"
    "${lua_SOURCE_DIR}/src/lmathlib.c"
    "${lua_SOURCE_DIR}/src/lmem.c"
    "${lua_SOURCE_DIR}/src/loadlib.c"
    "${lua_SOURCE_DIR}/src/lobject.c"
    "${lua_SOURCE_DIR}/src/lopcodes.c"
    "${lua_SOURCE_DIR}/src/loslib.c"
    "${lua_SOURCE_DIR}/src/lparser.c"
    "${lua_SOURCE_DIR}/src/lstate.c"
    "${lua_SOURCE_DIR}/src/lstring.c"
    "${lua_SOURCE_DIR}/src/lstrlib.c"
    "${lua_SOURCE_DIR}/src/ltable.c"
    "${lua_SOURCE_DIR}/src/ltablib.c"
    "${lua_SOURCE_DIR}/src/ltm.c"
    "${lua_SOURCE_DIR}/src/lundump.c"
    "${lua_SOURCE_DIR}/src/lutf8lib.c"
    "${lua_SOURCE_DIR}/src/lvm.c"
    "${lua_SOURCE_DIR}/src/lzio.c"
)
add_library(lua_static STATIC ${LUA_CORE_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}/src")
set_target_properties(lua_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
