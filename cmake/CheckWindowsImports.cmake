# Check the final PE import table, rather than relying on linker flags alone.
cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ONDRA_OBJDUMP OR NOT EXISTS "${ONDRA_EXECUTABLE}")
    message(FATAL_ERROR "Set ONDRA_OBJDUMP and ONDRA_EXECUTABLE to an objdump tool and an existing Windows EXE.")
endif()
execute_process(COMMAND "${ONDRA_OBJDUMP}" -p "${ONDRA_EXECUTABLE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE imports ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${ONDRA_EXECUTABLE}: ${error}")
endif()
string(REGEX MATCHALL "DLL Name:[ \t]*[^\r\n]+" import_lines "${imports}")
if(NOT import_lines)
    message(FATAL_ERROR "No PE imports found in ${ONDRA_EXECUTABLE}; cannot verify static dependencies.")
endif()
# Libraries supplied by Windows itself. UCRT64 uses UCRT/API-set imports;
# MINGW64 uses msvcrt.dll. SDL2, libstdc++, libgcc and libwinpthread are absent.
set(system_dlls
    advapi32.dll bcrypt.dll cfgmgr32.dll comctl32.dll comdlg32.dll crypt32.dll
    d3d9.dll d3d11.dll d3d12.dll dinput8.dll dsound.dll dwmapi.dll dxgi.dll
    gdi32.dll hid.dll imm32.dll kernel32.dll msvcrt.dll ntdll.dll ole32.dll
    oleaut32.dll powrprof.dll rpcrt4.dll secur32.dll setupapi.dll shell32.dll
    shlwapi.dll ucrtbase.dll user32.dll userenv.dll usp10.dll uxtheme.dll
    version.dll winmm.dll winspool.drv ws2_32.dll)
set(checked "")
foreach(line IN LISTS import_lines)
    string(REGEX REPLACE "^DLL Name:[ \t]*" "" dll "${line}")
    string(STRIP "${dll}" dll)
    string(TOLOWER "${dll}" dll)
    if(NOT dll IN_LIST system_dlls AND NOT dll MATCHES "^(api-ms-win-|ext-ms-win-)[a-z0-9-]+\\.dll$")
        message(FATAL_ERROR "Standalone build imports a non-system DLL: ${dll} (${ONDRA_EXECUTABLE})")
    endif()
    list(APPEND checked "${dll}")
endforeach()
get_filename_component(name "${ONDRA_EXECUTABLE}" NAME)
message(STATUS "${name}: verified Windows system DLLs only (${checked})")
