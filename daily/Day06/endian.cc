#include <iostream>

int main() {
    int num = 0x12345678;
    // little : 78 56 34 12
    // big    : 12 34 56 78
    if (reinterpret_cast<char*>(&num)[0] == 0x78) {
        std::cout << "Little Endian\n";
    } else {
        std::cout << "Big Endian\n";
    }
    return 0;
}

