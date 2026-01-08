#pragma once

#include "imgui.h"
#include "json.hpp"

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

using json = nlohmann::json;
namespace gui {

    class I18N {
    public:
        std::string currentLang;

        bool load(const std::string& resDir, const std::string& lang);

        const char* get(const char* key) {
            if (!key) return "";
            std::string k(key);
            auto it = translations.find(k);
            if (it != translations.end()) return it->second.c_str();
            return key;
        }

        std::vector<ImWchar> BuildGlyphRanges();

    private:
        std::unordered_map<std::string, std::string> translations;

        static uint16_t DecodeUTF8_BMP(const char*& p, const char* end);

        std::set<uint16_t> CollectUniqueBMP();
    };
}
