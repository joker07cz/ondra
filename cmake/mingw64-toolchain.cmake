# Optional Linux -> Windows cross compilation. Native MSYS2 builds do not need
# a toolchain file. Override MINGW_PREFIX for a relocated cross toolchain.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(MINGW_PREFIX "x86_64-w64-mingw32" CACHE STRING "MinGW compiler prefix, optionally with an absolute path")
set(CMAKE_C_COMPILER "${MINGW_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${MINGW_PREFIX}-g++")
set(CMAKE_RC_COMPILER "${MINGW_PREFIX}-windres")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
