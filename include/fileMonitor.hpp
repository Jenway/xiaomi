#pragma once
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

namespace fileMonitor {

enum class FileMode {
    Read,
    Write,
    Append
};

using FilePtr = std::unique_ptr<FILE, decltype(&fclose)>;

bool monitor(const std::string& filePath, FileMode mode, const std::function<void(FILE*)>& func);
std::string readFile(const std::string& filePath);
bool writeFile(const std::string& filePath, const std::string& content);
}