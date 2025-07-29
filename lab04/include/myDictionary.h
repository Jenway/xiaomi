#pragma once
#include <optional>
#include <string>
#include <unordered_map>

namespace dict {

class myDictionary {
public:
    void addEntry(const std::string& en, const std::string& zh);
    std::optional<std::string> translateToChinese(const std::string& en) const;
    std::optional<std::string> translateToEnglish(const std::string& zh) const;

private:
    std::unordered_map<std::string, std::string> enToZh;
    std::unordered_map<std::string, std::string> zhToEn;
};

template <typename Map>
concept StringMapLike = requires(Map& m, const std::string& key) {
    { m.find(key) } -> std::convertible_to<typename Map::const_iterator>;
    { m.end() } -> std::convertible_to<typename Map::const_iterator>;
    { m[key] } -> std::convertible_to<std::string>;
    { m.find(key)->second } -> std::convertible_to<std::string>;
};

template <StringMapLike Map>
std::optional<std::string> translate(const Map& map, const std::string& key)
{
    if (auto it = map.find(key); it != map.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace dict
