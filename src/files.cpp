// SPDX-License-Identifier: GPL-3.0-or-later
#include <ondra/files.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ondra {
#ifdef _WIN32
std::wstring utf16(const std::string& path) {
    if (path.empty()) return L"";
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), NULL, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 file name");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), &result[0], size);
    // Extended-length paths returned by GetFinalPathNameByHandleW require
    // backslashes even though our stored/canonical paths use '/'.
    std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}
std::string utf8(const std::wstring& path) {
    if (path.empty()) return "";
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), NULL, 0, NULL, NULL);
    if (!size) throw std::runtime_error("Invalid UTF-16 file name");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), &result[0], size, NULL, NULL);
    return result;
}
#endif
std::vector<std::uint8_t> readFile(const std::string& path, std::size_t limit) {
#ifdef _WIN32
    std::ifstream in(utf16(path).c_str(), std::ios::binary | std::ios::ate);
#else
    std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
#endif
    if (!in) throw std::runtime_error("Cannot open " + path);
    const std::streamoff size = in.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > limit) throw std::runtime_error("File too large: " + path);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), size)) throw std::runtime_error("Cannot read " + path);
    return data;
}
void writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
#ifdef _WIN32
    std::ofstream out(utf16(path).c_str(), std::ios::binary);
#else
    std::ofstream out(path.c_str(), std::ios::binary);
#endif
    if (!out || (!data.empty() && !out.write(reinterpret_cast<const char*>(data.data()), data.size())))
        throw std::runtime_error("Cannot write " + path);
    out.close();
    if (!out) throw std::runtime_error("Cannot close " + path);
}
std::uint16_t word(const std::vector<std::uint8_t>& d, std::size_t p) {
    if (p > d.size() || d.size() - p < 2) throw std::runtime_error("Truncated file");
    return static_cast<std::uint16_t>(d[p] | (d[p + 1] << 8));
}
std::uint32_t dword(const std::vector<std::uint8_t>& d, std::size_t p) {
    return word(d, p) | (static_cast<std::uint32_t>(word(d, p + 2)) << 16);
}
void putWord(std::vector<std::uint8_t>& d, std::uint16_t v) { d.push_back(v & 255); d.push_back(v >> 8); }
void putDword(std::vector<std::uint8_t>& d, std::uint32_t v) { putWord(d, v & 65535); putWord(d, v >> 16); }
std::string extension(const std::string& path) {
    const std::size_t p = path.find_last_of('.');
    std::string s = p == std::string::npos ? "" : path.substr(p);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string canonical(const std::string& path) {
#ifdef _WIN32
    HANDLE h = CreateFileW(utf16(path).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return "";
    std::vector<wchar_t> buffer(32768);
    DWORD n = GetFinalPathNameByHandleW(h, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (!n || n >= buffer.size()) return "";
    std::string result = utf8(std::wstring(buffer.data(), n));
    std::replace(result.begin(), result.end(), '\\', '/');
#else
    char* p = realpath(path.c_str(), NULL);
    if (!p) return "";
    std::string result(p);
    std::free(p);
#endif
    while (result.size() > 1 && result.back() == '/') result.pop_back();
    return result;
}
bool inside(const std::string& path, const std::string& root) {
    if (path.empty() || root.empty()) return false;
    std::string a = path, b = root;
#ifdef _WIN32
    for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
    return a == b || (a.size() > b.size() && a.compare(0, b.size(), b) == 0 && (b == "/" || a[b.size()] == '/'));
}
std::vector<FileEntry> directory(const std::string& path, bool showHidden) {
    std::vector<FileEntry> entries;
#ifdef _WIN32
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(utf16(path + "/*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return entries;
    do {
        std::string name = utf8(fd.cFileName);
        if (name.empty() || name == "." || name == ".." || (!showHidden && name[0] == '.')) continue;
        FileEntry e = {name, canonical(path + "/" + name), (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0,
            (static_cast<std::uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow};
        entries.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;
    while (dirent* de = readdir(dir)) {
        std::string name(de->d_name);
        if (name.empty() || name == "." || name == ".." || (!showHidden && name[0] == '.')) continue;
        const std::string p = path + "/" + name;
        struct stat st;
        if (stat(p.c_str(), &st) || (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))) continue;
        FileEntry e = {name, canonical(p), S_ISDIR(st.st_mode), static_cast<std::uint64_t>(st.st_size)};
        entries.push_back(e);
    }
    closedir(dir);
#endif
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) { return a.name < b.name; });
    return entries;
}
bool isDirectory(const std::string& path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(utf16(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}
}
