#pragma once

#include <string>
#include <vector>
#include "RandomUtils.h"

// Define a template class to handle the prize distribution
template <typename PrizeType>
class PrizeDistribution {
private:
    std::string maskName;
    std::vector<PrizeType> prizes;
    std::vector<int> weights;

public:
    // Default constructor
    PrizeDistribution() {}

    // Constructor with parameters
    PrizeDistribution(const std::string& mask, const std::vector<PrizeType>& prizeList, const std::vector<int>& weightList)
        : maskName(mask), prizes(prizeList), weights(weightList) {}

    PrizeType getRandomPrize() const {
        // Calculate total weight
        int totalWeight = 0;
        for (int weight : weights) {
            totalWeight += weight;
        }

        // Choose a random number
        int randIndex = getRandFromDist(maskName, weights);

        // Return the corresponding prize
        return prizes[randIndex];
    }

    // Change Prizes
    void setPrizes(const std::vector<PrizeType>& newPrizes) { prizes = newPrizes; }
    // Change Prizes by index
    void setPrize(int index, const PrizeType& newPrize) { prizes[index] = newPrize; }

    // Change Weights
    void setWeights(const std::vector<int>& newWeights) { weights = newWeights; }
    // Change Weights by index
    void setWeight(int index, int newWeight) { weights[index] = newWeight; }

    // Getters
    const std::vector<PrizeType>& getPrizes() const { return prizes; }
    const std::vector<int>& getWeights() const { return weights; }
};

