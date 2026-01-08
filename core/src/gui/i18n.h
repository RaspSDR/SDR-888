#pragma once

#include <string>
#include <fstream>
#include <unordered_map>
#include "json.hpp"

using json = nlohmann::json;
namespace gui {

    class i18n {
    public:
        static i18n& instance() {
            static i18n inst;
            return inst;
        }

        bool load(const std::string& resDir, const std::string& lang) {
            if (lang == "en") {
                translations.clear();
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
                return true;
            }
            return false;
        }

        const char* get(const char* key) {
            if (!key) return "";
            std::string k(key);
            auto it = translations.find(k);
            if (it != translations.end()) return it->second.c_str();
            return key;
        }

    private:
        std::unordered_map<std::string, std::string> translations;
    };
}

#define _L(key) gui::i18n::instance().get(key)