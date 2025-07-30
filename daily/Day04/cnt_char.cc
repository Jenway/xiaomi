#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_set>

namespace {
int32_t countDifferentChars(const std::string& str)
{
    auto char_set = std::unordered_set<char>();
    for (const auto& c : str) {
        char_set.insert(c);
    }
    return static_cast<int32_t>(char_set.size());
}
}

int main()
{
    auto str = std::string(std::istream_iterator<char>(std::cin),
        std::istream_iterator<char>());
    std::cout << "Diff chars in [" << str << "] = "
              << countDifferentChars(str) << '\n';
    return 0;
}