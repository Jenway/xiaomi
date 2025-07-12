#include "fileMonitor.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

namespace fileMonitor {

bool monitor(const std::string& filePath, FileMode mode, const std::function<void(FILE*)>& func)
{
    const char* modes[] = { "r", "w", "a" };
    FILE* rawFile = fopen(filePath.c_str(), modes[static_cast<int>(mode)]);
    if (!rawFile) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return false;
    }

    FilePtr file(rawFile, fclose);
    func(file.get());
    return true;
}

std::string readFile(const std::string& filePath)
{
    std::stringstream buffer;
    bool success = monitor(filePath, FileMode::Read, [&](FILE* file) {
        char temp[1024];
        while (fgets(temp, sizeof(temp), file)) {
            buffer << temp;
        }
    });
    return success ? buffer.str() : "";
}

bool writeFile(const std::string& filePath, const std::string& content)
{
    return monitor(filePath, FileMode::Write, [&](FILE* file) {
        fputs(content.c_str(), file);
    });
}
}