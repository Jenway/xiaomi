//
// Created by jenway on 25-7-21.
//

#ifndef CHARMAPPER_HPP
#define CHARMAPPER_HPP

#include <cstdint>
#include <algorithm>
#include<string>
#include<cstdio>

class CharMapper {
public:
    virtual ~CharMapper() = default;

    virtual const char* map(uint8_t gray, uint8_t r, uint8_t g, uint8_t b) const = 0;
    virtual bool isAscii() const { return true; }
};

class AsciiCharMapper : public CharMapper {
public:
    const char* map(uint8_t gray, uint8_t, uint8_t, uint8_t) const override {
        static constexpr char charset[] = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
        static constexpr int len = sizeof(charset) - 1;
        int index = static_cast<int>(gray * len / 255.0f);
        index = std::clamp(index, 0, len - 1);
        return &charset[index];
    }

    bool isAscii() const override { return true; }
};

class BrailleCharMapper : public CharMapper {
public:
    const char* map(uint8_t bits, uint8_t, uint8_t, uint8_t) const override {
        static thread_local char buf[5]; // UTF-8 最多 4 字节 + '\0'
        uint16_t codepoint = 0x2800 + bits;  // Braille 基础码位 + 点位掩码
        snprintf(buf, sizeof(buf), "%s", codepointToUTF8(codepoint).c_str());
        return buf;
    }

    bool isAscii() const override { return false; }

private:
    static std::string codepointToUTF8(uint32_t cp) {
        std::string out;
        if (cp <= 0x7F) out += char(cp);
        else if (cp <= 0x7FF) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
        return out;
    }
};

class UnicodeRampMapper : public CharMapper {
public:
    const char* map(uint8_t gray, uint8_t, uint8_t, uint8_t) const override {
        static const char* charset[] = {
            " ",    // U+0020
            "░",    // U+2591 Light Shade
            "▒",    // U+2592 Medium Shade
            "▓",    // U+2593 Dark Shade
            "▁",    // U+2581 Lower one eighth block
            "▂",    // U+2582
            "▃",    // U+2583
            "▄",    // U+2584
            "▅",    // U+2585
            "▆",    // U+2586
            "▇",    // U+2587
            "█"     // U+2588 Full block
        };
        static constexpr int len = sizeof(charset) / sizeof(char*);

        int index = static_cast<int>(gray * (len - 1) / 255.0f);
        return charset[std::clamp(index, 0, len - 1)];
    }

    bool isAscii() const override { return false; }
};


#endif //CHARMAPPER_HPP
