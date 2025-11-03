#ifndef RANDOM_UTILS_H
#define RANDOM_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <limits>

#include <random>
#include <numeric>
#include "RandomLogGenerator.h" // Include if you use RandomLogGenerator in these functions

extern int instructionIndex; // Same for instructionIndex if it's used globally

// Fast xorshift64* RNG suitable for non-crypto uses; lightweight and very fast.
struct XorShift64Star {
    using result_type = uint64_t;
    explicit XorShift64Star(uint64_t seed = 0) { if (seed == 0) seed = 0x9e3779b97f4a7c15ULL ^ 1; state = seed; }
    result_type operator()() {
        uint64_t x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 2685821657736338717ULL;
    }
    static constexpr result_type min() { return 0ULL; }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
private:
    uint64_t state;
};

// Thread-local fast RNG accessor: seeds once per thread and reuses the engine for all calls.
inline XorShift64Star& getThreadRng() {
    static thread_local XorShift64Star gen([]() -> uint64_t {
        std::random_device rd;
        uint64_t a = static_cast<uint64_t>(rd());
        uint64_t b = static_cast<uint64_t>(rd());
        return (a << 32) | b;
    }());
    return gen;
}

// Define a method to generate random numbers within a specified range
inline int getRand(const std::string& mask, int range) {
    int index;
    if (logMode == REPLAY) {
        std::vector<RandTriple> randomLogInstructions = RandomLogGenerator::getRandomLogInstructions();
        RandTriple currentInstruction = randomLogInstructions[instructionIndex++];
        if (instructionIndex >= randomLogInstructions.size()) {
            std::cout << "Error: End of log file" << std::endl;
            logMode = NO_LOGGING;
        }
        if (currentInstruction.mask != mask)
            std::cout << "Error: Mask mismatch  " << currentInstruction.mask + ":" + std::to_string(currentInstruction.result) + ":" + std::to_string(currentInstruction.range) <<
            " vs " << mask + ":" + std::to_string(currentInstruction.result) + ":" + std::to_string(range) << std::endl;
        else if (currentInstruction.range != range)
            std::cout << "Error: Range mismatch  " << currentInstruction.mask + ":" + std::to_string(currentInstruction.result) + ":" + std::to_string(currentInstruction.range) <<
            " vs " << mask + ":" + std::to_string(currentInstruction.result) + ":" + std::to_string(range) << std::endl;
        index = currentInstruction.result;
    }
    else {
        // Use a thread-local fast RNG seeded once per thread and reuse it across calls.
        XorShift64Star& gen = getThreadRng();
        std::uniform_int_distribution<> dis(0, range - 1);
        index = static_cast<int>(dis(gen));
        if (logMode == 1) {
            RandTriple randTriple = { mask, index, range };
            RandomLogGenerator::addRandom(randTriple);
        }
    }
    return index;
}

// Function to convert a discrete distribution into a uniform distribution,
// choose a random number, and output the chosen number, total weight, and chosen index
inline int getRandFromDist(const std::string& mask, const std::vector<int>& distribution) {
    int totalWeight = std::accumulate(distribution.begin(), distribution.end(), 0);
    int rnd = getRand(mask, totalWeight);
    int index = 0;
    int weightSum = 0;
    for (size_t i = 0; i < distribution.size(); ++i) {
        weightSum += distribution[i];
        if (rnd < weightSum) {
            index = i;
            break;
        }
    }
    return index;
}

// New method to randomly choose r positions from n
inline std::vector<int> getRandomPositions(const std::string& mask, int n, int r) {
    std::vector<int> positions(n);
    std::iota(positions.begin(), positions.end(), 0);

    std::vector<int> chosenPositions;
    for (int i = 0; i < r; ++i) {
        int chosenIndex = getRand(mask + "_" + std::to_string(i), n - i);
        chosenPositions.push_back(positions[chosenIndex]);
        std::swap(positions[chosenIndex], positions[n - i - 1]);
    }

    return chosenPositions;
}
#endif // RANDOM_UTILS_H
