#ifndef RANDOMLOGGENERATOR_H
#define RANDOMLOGGENERATOR_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;


struct RandTriple {
    std::string mask;
    int result;
    int range;
};

enum LogMode {
    NO_LOGGING,
    LOGGING,
    REPLAY
};
enum SimulationMode {
    RANDOM_MODE,
    PLAYER_MODE,
    CSV_MODE
};
extern LogMode logMode;
extern int instructionIndex;  // Index for replaying randoms

class RandomLogGenerator {
public:
    // File streams for logging
    static std::ofstream randomLogFile;
    static std::ofstream gameDetailsFile;

    // NEW: Toggle to choose logging mode.
    // When true, each tumble win (i.e. cascade win beyond the base win) is logged separately.
    // When false, wins are aggregated into one value.
    static bool logTumbleWinsIndividually;

    // Game-related state
    static int currentRound, currentSpin;
    static std::vector<std::string> currentRandoms;  // Stores randoms for a spin
    static double currentSpinTotalWin;               // Stores total win for a spin
    static double currentRoundTotalWin;              // Stores total win for the round
    static double maxRoundWin;                       // Max win for a round
    static bool maxWinTriggered;                     // Flag to indicate if max win was triggered
    static std::vector< std::vector<json>> roundScreens;
    static std::vector< std::vector<json>> roundScales;
    static std::vector<std::vector<int>> roundMultipliers;
    static std::vector<std::vector<double>> roundWheelBonusPrizes;  // Add this to store wheel bonus prizes for each spin

    // NEW: Container to hold the tumble wins for the current spin.
    static std::vector<double> currentSpinTumbleWins;

    // static bool loggingEnabled;

     // For replay mode
    static std::vector<RandTriple> randomLogInstructions;

    // Method declarations
    static void setMaxRoundWin(double maxWin);       // Set the maximum round win
    static void startRound();                        // Prepare for a new round
    static void endRound();                          // Finalize round, log randoms and game details
    static void startSpin();                         // Start logging a new spin
    static bool endSpin();                           // Finalize a spin, log randoms and win amounts, returns false if maxRoundWin is hit
    static bool newSpin();                           // Start a new spin, return false if maxRoundWin is hit

    static void addRandom(const RandTriple& randTriple);  // Add a random to the current spin
    static void addScreen(json screen);              // Log the screen for the current spin
    static void addScale(json scale);                // Log the scale for the current spin
    static void addWinAmount(double winAmount);      // Add a win to the current spin

    static void openLogs(const std::string& randomLogFileName, const std::string& gameDetailsFileName);
    static void closeLogs();


    static void addMultipliers(std::vector<int>& multipliersUsed);
    static void addWheelBonusPrizes(std::vector<double>& wheelBonusPrizes);

    // Existing method for logging a win—modify it so that it behaves differently
    // based on the logging mode.
    static void addTumbleWin(double winAmount);

    // Replay-related methods
    static void readAndParseLog(const std::string& filename);
    static std::vector<RandTriple> getRandomLogInstructions();


    static bool handleLoggingMode(LogMode mode, const std::string& randomLogFileName, const std::string& gameDetailsFileName);



private:
    static void parseLogLine(const std::string& line, std::vector<RandTriple>& entries);

};

// Method Definitions
// Initialize the toggle flag (default to false to preserve existing behavior)
bool RandomLogGenerator::logTumbleWinsIndividually = true;

std::ofstream RandomLogGenerator::randomLogFile;
std::ofstream RandomLogGenerator::gameDetailsFile;
int RandomLogGenerator::currentRound = 0;
int RandomLogGenerator::currentSpin = 0;
std::vector<std::string> RandomLogGenerator::currentRandoms;
double RandomLogGenerator::currentSpinTotalWin = 0.0;
double RandomLogGenerator::currentRoundTotalWin = 0.0;
double RandomLogGenerator::maxRoundWin = std::numeric_limits<double>::max();  // Default to no max win
bool RandomLogGenerator::maxWinTriggered = false;
std::vector<std::vector<json>> RandomLogGenerator::roundScreens;
std::vector<std::vector<json>> RandomLogGenerator::roundScales;
std::vector<RandTriple> RandomLogGenerator::randomLogInstructions;
//LogMode logMode;
int instructionIndex = 0;
std::vector<std::vector<int>> RandomLogGenerator::roundMultipliers;
std::vector<std::vector<double>> RandomLogGenerator::roundWheelBonusPrizes;

std::vector<double> RandomLogGenerator::currentSpinTumbleWins;

// Set the maximum win for a round
void RandomLogGenerator::setMaxRoundWin(double maxWin) {
    maxRoundWin = maxWin;
}

// Open log files
void RandomLogGenerator::openLogs(const std::string& randomLogFileName, const std::string& gameDetailsFileName) {
    if (logMode == LOGGING) {
        randomLogFile.open(randomLogFileName);
        gameDetailsFile.open(gameDetailsFileName);
    }
}

// Close log files
void RandomLogGenerator::closeLogs() {
    if (logMode == LOGGING) {
        randomLogFile.close();
        gameDetailsFile.close();
    }
}


// Handle different logging modes and file setup
bool RandomLogGenerator::handleLoggingMode(LogMode mode, const std::string& randomLogFileName, const std::string& gameDetailsFileName) {
    logMode = mode;
    instructionIndex = 0;

    if (logMode == LOGGING) {
        openLogs(randomLogFileName, gameDetailsFileName);
        return true;
    }

    if (logMode == REPLAY) {
        readAndParseLog(randomLogFileName);  // Read the log file for replay
        // Open only the gameDetailsFile for writing in REPLAY mode
        gameDetailsFile.open(gameDetailsFileName);
        return !randomLogInstructions.empty();
    }

    if (logMode == NO_LOGGING) {
        return false;  // No logging or replay, just execute normally
    }

    return false;
}

// Start a new round
void RandomLogGenerator::startRound() {
    if (logMode == LOGGING || logMode == REPLAY) {
        currentRandoms.clear();
        roundScreens.clear();
        roundScales.clear();
        roundMultipliers.clear();
        roundWheelBonusPrizes.clear();
        currentSpinTotalWin = 0.0;
        currentRoundTotalWin = 0.0;
        maxWinTriggered = false;
        currentSpin = 0;
        currentRound++;

        startSpin();
    }
}


void RandomLogGenerator::endRound() {
    if (logMode == NO_LOGGING) return;
    
    endSpin();  // Finalize the last spin

    // Log the total round win to the random log (cap the win if maxRoundWin is hit)
    double totalWin = maxWinTriggered ? maxRoundWin : currentRoundTotalWin;
    randomLogFile << "#" << std::fixed << std::setprecision(2) << totalWin / 100 << std::endl;
    

    // Log the game details (screen state) to gameDetails.txt
    gameDetailsFile << "{" << std::endl;
    for (size_t i = 0; i < currentSpin; ++i) {
        gameDetailsFile << "  \"spin_" << i << "\": [" << std::endl;
        gameDetailsFile << "  \"Screen" << "\": [" << std::endl;
        for (size_t screenIdx = 0; screenIdx < roundScreens[i].size(); ++screenIdx) {
            for (size_t rowIdx = 0; rowIdx < roundScreens[i][screenIdx].size(); ++rowIdx) {
                const auto& row = roundScreens[i][screenIdx][rowIdx];
                gameDetailsFile << "    [";
                for (size_t j = 0; j < row.size(); ++j) {
                    gameDetailsFile << row[j];  // Directly write the symbol without additional quotes
                    if (j < row.size() - 1) gameDetailsFile << ", ";
                }
                gameDetailsFile << "]";
                if (rowIdx < roundScreens[i][screenIdx].size() - 1) {
                    gameDetailsFile << ",";
                }
                gameDetailsFile << std::endl;
            }
            gameDetailsFile << "  ]";
            if (screenIdx < roundScreens[i].size() - 1)
                gameDetailsFile << ",";

            gameDetailsFile << std::endl;
        }

        // gameDetailsFile << "}" << std::endl;
        if (i < currentSpin - 1) {
            gameDetailsFile << ",";
        }
        gameDetailsFile << std::endl;
    }

    gameDetailsFile << "}" << std::endl;
    gameDetailsFile << "========== end round: " << currentRound << " ===========" << std::endl;
}

// Start a new spin
void RandomLogGenerator::startSpin() {
    if (logMode == NO_LOGGING) return;
    currentRandoms.clear();
    currentSpinTotalWin = 0.0;
    currentSpin++;
    roundScales.push_back({});
    roundScreens.push_back({});

    currentSpinTumbleWins.clear();
}

// End the spin and log its data
bool RandomLogGenerator::endSpin() {
    if (logMode == NO_LOGGING) return true;

    if (logMode == LOGGING) {
        // Log randoms and total win for the spin
        std::string randomsLine = std::accumulate(currentRandoms.begin(), currentRandoms.end(), std::string(),
            [](const std::string& a, const std::string& b) -> std::string {
                return a.empty() ? b : a + "," + b;
            });

        if (logTumbleWinsIndividually) {
            // Output only the individual cascade wins.
            for (double tumbleWin : currentSpinTumbleWins) {
                std::ostringstream ossTumble;
                ossTumble << std::fixed << std::setprecision(2) << tumbleWin / 100;
                randomsLine += ",#" + ossTumble.str();
            }
            randomsLine += ";";
            // (Do not append aggregated currentSpinTotalWin)
        }
        else {
            // In aggregated mode, output the overall spin win.
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << currentSpinTotalWin / 100;
            randomsLine += ",#" + oss.str() + ";";
        }


        randomLogFile << randomsLine;
    }
    // Add the spin's win to the total round win
    currentRoundTotalWin += currentSpinTotalWin;

    // Check if maxRoundWin is hit or exceeded
    if (currentRoundTotalWin >= maxRoundWin) {
        // Log the full spin win even if it exceeds maxRoundWin
        currentRoundTotalWin = maxRoundWin;
        maxWinTriggered = true;
        return false;  // Signal that max round win was hit
    }

    return true;  // Continue the round
}

bool RandomLogGenerator::newSpin() {
    bool x = endSpin();
    startSpin();
    return x;
}

// Add random result
void RandomLogGenerator::addRandom(const RandTriple& randTriple) {
    if (logMode == LOGGING) {
        currentRandoms.push_back(randTriple.mask + ":" + std::to_string(randTriple.result) + ":" + std::to_string(randTriple.range));
    }
}

// Add screen state
void RandomLogGenerator::addScreen(json screen) {
    if (logMode != NO_LOGGING) {
        roundScreens[currentSpin - 1].push_back(screen);
    }
}

// Add win to the spin total
void RandomLogGenerator::addWinAmount(double winAmount) {
    if (logMode == LOGGING || logMode == REPLAY) {
        if (logTumbleWinsIndividually) {
            // In individual mode, simply add a new entry for this cascade win.
            currentSpinTumbleWins.push_back(winAmount);
        }
        else {
            // In aggregated mode, accumulate all wins into one entry.
            if (currentSpinTumbleWins.empty()) {
                currentSpinTumbleWins.push_back(winAmount);
            }
            else {
                currentSpinTumbleWins.back() += winAmount;
            }
        }
        // In either case, add the win amount to the overall spin total.
        currentSpinTotalWin += winAmount;
    }
}

// Modify addTumbleWin so that it “accumulates” differently depending on the flag.
//void RandomLogGenerator::addTumbleWin(double winAmount) {
//    if (logTumbleWinsIndividually) {
//        // In individual mode, simply add a new entry for this cascade win.
//        currentSpinTumbleWins.push_back(winAmount);
//    }
//    else {
//        // In aggregated mode, accumulate all wins into one entry.
//        if (currentSpinTumbleWins.empty()) {
//            currentSpinTumbleWins.push_back(winAmount);
//        }
//        else {
//            currentSpinTumbleWins.back() += winAmount;
//        }
//    }
//    // In either case, add the win amount to the overall spin total.
//    currentSpinTotalWin += winAmount;
//
//}

void RandomLogGenerator::addMultipliers(std::vector<int>& multipliersUsed) {
    if (logMode == LOGGING) {
        roundMultipliers.push_back(multipliersUsed);
    }
}

void RandomLogGenerator::addWheelBonusPrizes(std::vector<double>& wheelBonusPrizes) {
    if (logMode == LOGGING && !wheelBonusPrizes.empty()) {
        roundWheelBonusPrizes.push_back(wheelBonusPrizes);  // Add only non-empty bonuses
    }
    else {
        roundWheelBonusPrizes.push_back({});  // Add an empty vector to maintain order in spins
    }
}

void RandomLogGenerator::readAndParseLog(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        throw std::runtime_error("Could not open the log file: " + filename);
    }

    randomLogInstructions.clear();

    while (std::getline(file, line)) {
        std::vector<RandTriple> entries;
        parseLogLine(line, entries);
        randomLogInstructions.insert(randomLogInstructions.end(), entries.begin(), entries.end());
    }
}

std::vector<RandTriple> RandomLogGenerator::getRandomLogInstructions() {
    return randomLogInstructions;
}

void RandomLogGenerator::parseLogLine(const std::string& line, std::vector<RandTriple>& entries) {
    std::stringstream ss(line);
    std::string token;

    while (std::getline(ss, token, ';')) {
        std::stringstream tokenStream(token);
        std::string part;

        while (std::getline(tokenStream, part, '#')) {
            if (part.empty()) {
                continue;
            }
            std::vector<std::string> randTriples;
            std::stringstream randTripleStream(part);
            while (std::getline(randTripleStream, part, ',')) {
                randTriples.push_back(part);
            }
            for (const auto& randTriple : randTriples) {
                std::vector<std::string> parts;
                std::stringstream randTripleSS(randTriple);
                while (std::getline(randTripleSS, part, ':')) {
                    parts.push_back(part);
                }
                if (parts.size() == 3) {
                    RandTriple entry{ parts[0], std::stoi(parts[1]), std::stoi(parts[2]) };
                    entries.push_back(entry);
                }
            }
        }
    }
}

#endif // RANDOMLOGGENERATOR_H
