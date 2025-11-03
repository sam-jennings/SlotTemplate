#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <random>
#include "RandomUtils.h"

struct Symbol {
    std::string name;
    int counter;
    double value; // For PAY symbols if needed

    Symbol() : name(""), counter(1), value(0.0) {}
    Symbol(const std::string& name, int counter = 1, double value = 0.0)
        : name(name), counter(counter), value(value) {
    }
};

class SymbolStructure {
private:
    std::vector<std::string> symbols;
    std::vector<std::vector<int>> paytable_vec;
    std::map<std::string, std::vector<int>> paytable;
    std::vector<int> scatterPrizes;
    std::unordered_map<std::string, std::vector<std::string>> wildSubstitutions;

public:
    SymbolStructure() = default;

    SymbolStructure(const std::vector<std::string>& symbolNames, const std::vector<std::vector<int>>& symbolPayouts) {
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            symbols.push_back(symbolNames[i]);
            paytable_vec.push_back(symbolPayouts[i]);
            paytable[symbolNames[i]] = symbolPayouts[i];
        }
    }

    SymbolStructure(const std::vector<std::string>& symbolNames,
        const std::vector<std::vector<int>>& symbolPayouts,
        const std::unordered_map<std::string, std::vector<std::string>>& wildSubs)
        : symbols(symbolNames), paytable_vec(symbolPayouts), wildSubstitutions(wildSubs) {
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            paytable[symbolNames[i]] = symbolPayouts[i];
        }
    }

    // Check if a symbol is wild and get its substitutions
    std::vector<std::string> getWildSubstitutions(const std::string& wildSymbol) const {
        auto it = wildSubstitutions.find(wildSymbol);
        if (it != wildSubstitutions.end()) {
            return it->second;
        }
        return {};
    }

    const std::vector<std::string>& getSymbols() const { return symbols; }
    const std::vector< std::vector<int>>& getPaytableVec() const { return paytable_vec; }
    const std::map<std::string, std::vector<int>>& getPaytable() const { return paytable; }
    const std::vector<int>& getScatterPrizes() const { return scatterPrizes; }

    // Additional functionality for SymbolStructure can go here
    // For example, a method to find a symbol by name and return its index or payouts
    int findSymbolIndex(const std::string& name) const {
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (symbols[i] == name) return i;
        }
        return -1; // Symbol not found
    }

    const std::vector<int>* findSymbolPayouts(const std::string& name) const {
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (symbols[i] == name) return &paytable_vec[i];
        }
        return nullptr; // Symbol not found
    }

    const int getNumSymbols() const {
        return symbols.size();
    }

    const int getWinLength() const {
        return paytable_vec[0].size();
    }
};

struct Reel {
    std::vector<std::string> symbols;
    std::vector<int> weights;

    // Constructor to accept a vector of strings
    Reel(const std::vector<std::string>& _symbols, const std::vector<int>& _weights = {})
        : symbols(_symbols), weights(_weights) {
    }

    bool isWeighted() const { return !weights.empty(); }
};

class ReelSet {
private:
    std::string mask;

    // Optional over/under reels
    std::unique_ptr<Reel> overReel;
    std::unique_ptr<Reel> underReel;
    std::string overMask;
    std::string underMask;

public:
    std::vector<Reel> reels;
    std::vector<int> currentIndices; // Store current indices
    int currentOverIndex = 0;  // Store current over reel index
    int currentUnderIndex = 0; // Store current under reel index

    // Constructor for backward compatibility (no over/under reels)
    ReelSet(const std::vector<Reel>& reels, const std::string& mask)
        : reels(reels), mask(mask), currentIndices(reels.size(), 0) {
    }

    // Constructor with optional over/under reels
    ReelSet(const std::vector<Reel>& reels, const std::string& mask,
        const Reel* overReel, const std::string& overMask,
        const Reel* underReel, const std::string& underMask)
        : reels(reels), mask(mask), currentIndices(reels.size(), 0),
        overMask(overMask), underMask(underMask) {
        if (overReel) {
            this->overReel = std::make_unique<Reel>(*overReel);
        }
        if (underReel) {
            this->underReel = std::make_unique<Reel>(*underReel);
        }
    }

    // Default constructor
    ReelSet() {}

    // Copy constructor
    ReelSet(const ReelSet& other)
        : reels(other.reels), mask(other.mask), currentIndices(other.currentIndices),
        overMask(other.overMask), underMask(other.underMask),
        currentOverIndex(other.currentOverIndex), currentUnderIndex(other.currentUnderIndex) {
        if (other.overReel) {
            overReel = std::make_unique<Reel>(*other.overReel);
        }
        if (other.underReel) {
            underReel = std::make_unique<Reel>(*other.underReel);
        }
    }

    // Copy assignment operator
    ReelSet& operator=(const ReelSet& other) {
        if (this != &other) {
            reels = other.reels;
            mask = other.mask;
            currentIndices = other.currentIndices;
            overMask = other.overMask;
            underMask = other.underMask;
            currentOverIndex = other.currentOverIndex;
            currentUnderIndex = other.currentUnderIndex;

            if (other.overReel) {
                overReel = std::make_unique<Reel>(*other.overReel);
            }
            else {
                overReel.reset();
            }

            if (other.underReel) {
                underReel = std::make_unique<Reel>(*other.underReel);
            }
            else {
                underReel.reset();
            }
        }
        return *this;
    }

    // Move constructor
    ReelSet(ReelSet&& other) noexcept = default;

    // Move assignment operator
    ReelSet& operator=(ReelSet&& other) noexcept = default;

    // Check if this reelset has over/under reels
    bool hasOverReel() const { return overReel != nullptr; }
    bool hasUnderReel() const { return underReel != nullptr; }

    // Get over/under reels
    const Reel* getOverReel() const { return overReel.get(); }
    const Reel* getUnderReel() const { return underReel.get(); }

    // Calculate complete cycle
    int getCycle() const {
        int cycle = 1;
        for (const auto& reel : reels) {
            cycle *= reel.isWeighted() ? std::accumulate(reel.weights.begin(), reel.weights.end(), 0) : reel.symbols.size();
        }
        return cycle;
    }

    // Spin reels method - now also spins over/under if they exist
    void spinReels() {
        std::vector<int> chosenIndices;
        for (int reelIndex = 0; reelIndex < reels.size(); ++reelIndex) {
            int index;
            if (reels[reelIndex].isWeighted()) {
                // Use weighted distribution
                index = getRandFromDist(mask, reels[reelIndex].weights);
            }
            else {
                // Use uniform distribution
                index = getRand(mask, reels[reelIndex].symbols.size());
            }
            chosenIndices.push_back(index);
        }
        currentIndices = chosenIndices;

        // Spin over reel if it exists
        if (overReel) {
            if (overReel->isWeighted()) {
                currentOverIndex = getRandFromDist(overMask, overReel->weights);
            }
            else {
                currentOverIndex = getRand(overMask, overReel->symbols.size());
            }
        }

        // Spin under reel if it exists
        if (underReel) {
            if (underReel->isWeighted()) {
                currentUnderIndex = getRandFromDist(underMask, underReel->weights);
            }
            else {
                currentUnderIndex = getRand(underMask, underReel->symbols.size());
            }
        }
    }

    // Get current symbol from over/under reels
    std::string getCurrentOverSymbol() const {
        if (overReel && currentOverIndex < overReel->symbols.size()) {
            return overReel->symbols[currentOverIndex];
        }
        return "";
    }

    std::string getCurrentUnderSymbol() const {
        if (underReel && currentUnderIndex < underReel->symbols.size()) {
            return underReel->symbols[currentUnderIndex];
        }
        return "";
    }
};