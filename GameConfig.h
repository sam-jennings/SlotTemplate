#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

#include "nlohmann/json.hpp"
using nlohmann::json;

#include "Symbols.h"
#include "PrizeDistribution.h"

enum class GameMode { WAYS, LINES };

class Stats; // fwd


struct GameFlags {
    GameMode mode;
    bool cascades;
    bool megaways;
};

class GameConfig {
private:
    std::string filename;
    std::mutex config_mutex;
    json config_json;

    std::vector<std::string> rtpHeaders;

    static GameMode parseMode(const std::string& s) {
        if (s == "ways" || s == "WAYS") return GameMode::WAYS;
        if (s == "lines" || s == "LINES") return GameMode::LINES;
        throw std::invalid_argument("game.mode must be 'ways' or 'lines'");
    }


public:
    GameConfig(const std::string& filename) : filename(filename) {
        std::lock_guard<std::mutex> lock(config_mutex);
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open " + filename);
        }
        file >> config_json;
        parseRTPHeaders();
    }

    template<typename T>
    T parseVar(const std::string& key) {
        std::lock_guard<std::mutex> lock(config_mutex);
        if (!config_json.contains(key)) {
            throw std::invalid_argument("Key not found: " + key);
        }
        return config_json[key].get<T>();
    }

    template<typename T>
    T parseVarPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(config_mutex);
        json* node = &config_json;
        std::istringstream ss(path);
        std::string part;
        while (std::getline(ss, part, '/')) node = &(*node)[part];
        return node->get<T>();
    }

    template<typename T>
    std::vector<T> parseVec(const std::string& key, std::string subLevel = "") {
        if (!config_json.contains(key)) throw std::invalid_argument("Key not found: " + key);
        if (!subLevel.empty()) return config_json[key][subLevel].get<std::vector<T>>();
        return config_json[key].get<std::vector<T>>();
    }

    template<typename T>
    std::vector<std::vector<T>> parseArray(const std::string& key) {
        std::lock_guard<std::mutex> lock(config_mutex);
        if (!config_json.contains(key)) throw std::invalid_argument("Key not found: " + key);
        return config_json[key].get<std::vector<std::vector<T>>>();
    }

    void parseRTPHeaders() { rtpHeaders = parseVec<std::string>("payHeaders"); }
    const std::vector<std::string>& getRTPHeaders() const { return rtpHeaders; }

    std::vector<std::string> getGameInfo() {
        std::vector<std::string> info;
        info.push_back(config_json["game"]["gameName"].get<std::string>());
        info.push_back(config_json["game"]["RTP"].get<std::string>());
        // third field is “gameMode” label you used in output naming
        info.push_back(config_json["game"].contains("mode") ? config_json["game"]["mode"].get<std::string>() : "ways");
        return info;
    }

    GameFlags getGameFlags() {
        GameFlags f;
        auto& g = config_json["game"];
        f.mode = parseMode(g["mode"].get<std::string>());
        f.cascades = g["cascades"].get<bool>();
        f.megaways = g["megaways"].get<bool>();
        return f;
    }

    // Game core
    int getReels() { return config_json["game"]["reels"].get<int>(); }
    int getCost() { return config_json["game"]["cost"].get<int>(); }
    std::string getRTPKey() { return config_json["game"]["RTP"].get<std::string>(); }

    SymbolStructure parseSymbolStructure() {
        std::vector<std::string> symbols;
        std::vector<std::vector<int>> paytable;
        std::unordered_map<std::string, std::vector<std::string>> wildSubs;

        std::vector<std::string> symbolNames = config_json["paytable"]["symbols"].get<std::vector<std::string>>();
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            symbols.push_back(symbolNames[i]);
            std::vector<int> payout = config_json["paytable"]["pays"][symbolNames[i]].get<std::vector<int>>();
            paytable.push_back(payout);
        }
        for (const auto& item : config_json["paytable"]["wildSubs"].items()) {
            wildSubs[item.key()] = item.value().get<std::vector<std::string>>();
        }
        return SymbolStructure(symbols, paytable, wildSubs);
    }

    ReelSet parseReelSet(const std::string& reelSetName, std::string maskName = "") {
        auto& reelSetConfig = config_json["reel_sets"];
        for (auto& item : reelSetConfig) {
            if (item["name"] == reelSetName) {
                std::vector<Reel> reels;
                auto& reelsConfig = item["reels"];
                for (auto& reelConfig : reelsConfig) {
                    std::vector<std::string> symbols = reelConfig["symbols"].get<std::vector<std::string>>();
                    std::vector<int> weights;
                    if (reelConfig.contains("weights")) weights = reelConfig["weights"].get<std::vector<int>>();
                    reels.push_back(Reel(symbols, weights));
                }
                std::string mask = maskName.empty() ? static_cast<std::string>(item["mask"]) : maskName;

                Reel* overReel = nullptr;
                Reel* underReel = nullptr;
                std::string overMask = "", underMask = "";
                if (item.contains("overReel")) {
                    auto& o = item["overReel"];
                    std::vector<int> w; if (o.contains("weights")) w = o["weights"].get<std::vector<int>>();
                    overReel = new Reel(o["symbols"].get<std::vector<std::string>>(), w);
                    overMask = o.contains("mask") ? static_cast<std::string>(o["mask"]) : mask + "_OVER";
                }
                if (item.contains("underReel")) {
                    auto& u = item["underReel"];
                    std::vector<int> w; if (u.contains("weights")) w = u["weights"].get<std::vector<int>>();
                    underReel = new Reel(u["symbols"].get<std::vector<std::string>>(), w);
                    underMask = u.contains("mask") ? static_cast<std::string>(u["mask"]) : mask + "_UNDER";
                }

                ReelSet result(reels, mask, overReel, overMask, underReel, underMask);
                delete overReel; delete underReel;
                return result;
            }
        }
        throw std::invalid_argument("ReelSet not found: " + reelSetName);
    }

    std::unordered_map<std::string, ReelSet> parseAllReelSets() {
        std::unordered_map<std::string, ReelSet> reelSetsMap;
        for (auto& item : config_json["reel_sets"]) {
            std::string name = item["name"];
            reelSetsMap[name] = parseReelSet(name);
        }
        return reelSetsMap;
    }

    // Prize distributions by name or vector
    template <typename PrizeType>
    PrizeDistribution<PrizeType> parsePrizeDistribution(const std::string& prizeDistName, std::string subLevel = "") {
        json prizeDistConfig = config_json;
        if (!subLevel.empty()) {
            std::istringstream subLevelStream(subLevel);
            std::string level;
            while (getline(subLevelStream, level, '/')) prizeDistConfig = prizeDistConfig[level];
        }
        prizeDistConfig = prizeDistConfig[prizeDistName];

        std::string mask = prizeDistConfig["mask"];
        std::vector<PrizeType> prizes = prizeDistConfig["prizes"].get<std::vector<PrizeType>>();
        std::vector<int> weights = prizeDistConfig.contains("weights")
            ? prizeDistConfig["weights"].get<std::vector<int>>()
            : std::vector<int>(prizes.size(), 1);
        return PrizeDistribution<PrizeType>(mask, prizes, weights);
    }

    template <typename PrizeType>
    std::vector<PrizeDistribution<PrizeType>> parsePDVec(const std::string& key) {
        std::vector<PrizeDistribution<PrizeType>> v;
        json& node = config_json[key];
        for (auto& item : node.items()) {
            std::string k = item.key();
            v.push_back(parsePrizeDistribution<PrizeType>(k, key));
        }
        return v;
    }
};
