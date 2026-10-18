// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ondra {
std::vector<std::uint8_t> readFile(const std::string& path, std::size_t limit = 64 * 1024 * 1024);
void writeFile(const std::string& path, const std::vector<std::uint8_t>& data);
std::uint16_t word(const std::vector<std::uint8_t>& data, std::size_t pos);
std::uint32_t dword(const std::vector<std::uint8_t>& data, std::size_t pos);
void putWord(std::vector<std::uint8_t>& data, std::uint16_t value);
void putDword(std::vector<std::uint8_t>& data, std::uint32_t value);
std::string extension(const std::string& path);
struct FileEntry { std::string name, path; bool directory; std::uint64_t size; };
std::string canonical(const std::string& path);
std::vector<FileEntry> directory(const std::string& path);
bool inside(const std::string& path, const std::string& root);
bool isDirectory(const std::string& path);
#ifdef _WIN32
std::wstring utf16(const std::string& path);
std::string utf8(const std::wstring& path);
#endif
}
