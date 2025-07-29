#include "fileMonitor.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
    const std::string path = "example.txt";

    if (fileMonitor::writeFile(path, "Hello, world!\nSecond line.\n")) {
        std::cout << "Write succeeded.\n";
    }

    std::string content = fileMonitor::readFile(path);
    if (!content.empty()) {
        std::cout << "File content:\n"
                  << content << std::endl;
    } else {
        std::cout << "Failed to read file or file is empty.\n";
    }

    return 0;
}
