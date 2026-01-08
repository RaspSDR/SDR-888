#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <unordered_map>
#include "json.hpp"
#include "imgui.h"
#include "i18n.h"
#include "utils/flog.h"

using json = nlohmann::json;
namespace gui {
    bool I18N::load(const std::string& resDir, const std::string& lang) {
        if (lang == "en") {
            translations.clear();
            currentLang = "en";
            return true;
        }

        std::string path = resDir + "/locales/" + lang + ".json";
        std::ifstream f(path);
        if (f.is_open()) {
            json j;
            f >> j;
            translations.clear();
            for (auto& el : j.items()) {
                translations[el.key()] = el.value().get<std::string>();
            }
            currentLang = lang;
            return true;
        }
        return false;
    }

    std::vector<ImWchar> I18N::BuildGlyphRanges() {
        std::vector<ImWchar> ranges;

        auto cps = CollectUniqueBMP();

        if (cps.empty()) {
            ranges.push_back(0);
            return ranges;
        }

        auto it = cps.begin();
        uint16_t range_start = *it;
        uint16_t prev = range_start;

        flog::info("I18N: Building glyph ranges for {} code points", cps.size());
        ++it;
        for (; it != cps.end(); ++it) {
            uint16_t cp = *it;

            if (cp != prev + 1) {
                // close previous range
                ranges.push_back(range_start);
                ranges.push_back(prev);
                range_start = cp;
            }

            prev = cp;
        }

        // close last range
        ranges.push_back(range_start);
        ranges.push_back(prev);

        ranges.push_back(0); // terminator
        return ranges;
    }

    uint16_t I18N::DecodeUTF8_BMP(const char*& p, const char* end) {
        unsigned char c = *p++;

        if (c < 0x80)
            return c;

        if ((c >> 5) == 0x6) { // 110xxxxx
            if (p >= end) return 0xFFFD;
            uint16_t c2 = *p++;
            return ((c & 0x1F) << 6) | (c2 & 0x3F);
        }

        if ((c >> 4) == 0xE) { // 1110xxxx
            if (p + 1 >= end) return 0xFFFD;
            uint16_t c2 = *p++;
            uint16_t c3 = *p++;
            return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }

        // Anything else would be 4‑byte UTF‑8 → outside BMP → not allowed
        return 0xFFFD;
    }

    std::set<uint16_t> I18N::CollectUniqueBMP() {
        std::set<uint16_t> out;

        for (auto& kv : translations) {
            const std::string& s = kv.second;
            const char* p = s.data();
            const char* end = p + s.size();

            while (p < end) {
                uint16_t cp = DecodeUTF8_BMP(p, end);
                out.insert(cp);
            }
        }

        return out;
    }
}
