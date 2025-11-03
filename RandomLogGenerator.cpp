#include "RandomLogGenerator.h"
#include <numeric>
#include <iomanip>

// Global variables
//LogMode logMode;
int instructionIndex = 0;

// Static member initialization
bool RandomLogGenerator::logTumbleWinsIndividually = true;
std::ofstream RandomLogGenerator::randomLogFile;
std::ofstream RandomLogGenerator::gameDetailsFile;
int RandomLogGenerator::currentRound = 0;
int RandomLogGenerator::currentSpin = 0;
std::vector<std::string> RandomLogGenerator::currentRandoms;
double RandomLogGenerator::currentSpinTotalWin = 0.0;
double RandomLogGenerator::currentRoundTotalWin = 0.0;
double RandomLogGenerator::maxRoundWin = std::numeric_limits<double>::max();
bool RandomLogGenerator::maxWinTriggered = false;
std::vector<std::vector<json>> RandomLogGenerator::roundScreens;
std::vector<std::vector<json>> RandomLogGenerator::roundScales;
std::vector<std::vector<int>> RandomLogGenerator::roundMultipliers;
std::vector<std::vector<double>> RandomLogGenerator::roundWheelBonusPrizes;
std::vector<double> RandomLogGenerator::currentSpinTumbleWins;
std::vector<RandTriple> RandomLogGenerator::randomLogInstructions;

// Method definitions
void RandomLogGenerator::setMaxRoundWin(double maxWin) { maxRoundWin = maxWin; }

void RandomLogGenerator::openLogs(const std::string& randomLogFileName, const std::string& gameDetailsFileName) {
    if (logMode == LOGGING) {
        randomLogFile.open(randomLogFileName);
        gameDetailsFile.open(gameDetailsFileName);
    }
}

void RandomLogGenerator::closeLogs() {
    if (logMode == LOGGING) {
        randomLogFile.close();
        gameDetailsFile.close();
    }
}

bool RandomLogGenerator::handleLoggingMode(LogMode mode, const std::string& randomLogFileName, const std::string& gameDetailsFileName) {
    logMode = mode;
    instructionIndex = 0;

    if (logMode == LOGGING) {
        openLogs(randomLogFileName, gameDetailsFileName);
        return true;
    }

    if (logMode == REPLAY) {
        readAndParseLog(randomLogFileName);
        gameDetailsFile.open(gameDetailsFileName);
        return !randomLogInstructions.empty();
    }

    return false;
}

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
    if (logMode == LOGGING) return;

    endSpin();

    if (logMode == LOGGING) {
        double totalWin = maxWinTriggered ? maxRoundWin : currentRoundTotalWin;
        randomLogFile << "#" << std::fixed << std::setprecision(2) << totalWin / 100 << std::endl;
    }

    gameDetailsFile << "{" << std::endl;
    for (size_t i = 0; i < currentSpin; ++i) {
        gameDetailsFile << "  \"spin_" << i << "\": [" << std::endl;
        gameDetailsFile << "  \"Screen" << "\": [" << std::endl;
        for (size_t screenIdx = 0; screenIdx < roundScreens[i].size(); ++screenIdx) {
            for (size_t rowIdx = 0; rowIdx < roundScreens[i][screenIdx].size(); ++rowIdx) {
                const auto& row = roundScreens[i][screenIdx][rowIdx];
                gameDetailsFile << "    [";
                for (size_t j = 0; j < row.size(); ++j) {
                    gameDetailsFile << row[j];
                    if (j < row.size() - 1) gameDetailsFile << ", ";
                }
                gameDetailsFile << "]";
                if (rowIdx < roundScreens[i][screenIdx].size() - 1) gameDetailsFile << ",";
                gameDetailsFile << std::endl;
            }
            gameDetailsFile << "  ]";
            if (screenIdx < roundScreens[i].size() - 1) gameDetailsFile << ",";
            gameDetailsFile << std::endl;
        }
        if (i < currentSpin - 1) gameDetailsFile << ",";
        gameDetailsFile << std::endl;
    }
    gameDetailsFile << "}" << std::endl;
    gameDetailsFile << "========== end round: " << currentRound << " ===========" << std::endl;
}

void RandomLogGenerator::startSpin() {
    if (logMode == NO_LOGGING) return;
    currentRandoms.clear();
    currentSpinTotalWin = 0.0;
    currentSpin++;
    roundScales.push_back({});
    roundScreens.push_back({});
    currentSpinTumbleWins.clear();
}

bool RandomLogGenerator::endSpin() {
    if (logMode == NO_LOGGING) return true;

    if (logMode == LOGGING) {
        std::string randomsLine = std::accumulate(currentRandoms.begin(), currentRandoms.end(), std::string(),
            [](const std::string& a, const std::string& b) { return a.empty() ? b : a + "," + b; });

        if (logTumbleWinsIndividually) {
            for (double tumbleWin : currentSpinTumbleWins) {
                std::ostringstream ossTumble;
                ossTumble << std::fixed << std::setprecision(2) << tumbleWin / 100;
                randomsLine += ",#" + ossTumble.str();
            }
            randomsLine += ";";
        }
        else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << currentSpinTotalWin / 100;
            randomsLine += ",#" + oss.str() + ";";
        }
        randomLogFile << randomsLine;
    }

    currentRoundTotalWin += currentSpinTotalWin;
    if (currentRoundTotalWin >= maxRoundWin) {
        currentRoundTotalWin = maxRoundWin;
        maxWinTriggered = true;
        return false;
    }
    return true;
}

bool RandomLogGenerator::newSpin() {
    bool x = endSpin();
    startSpin();
    return x;
}

void RandomLogGenerator::addRandom(const RandTriple& randTriple) {
    if (logMode == LOGGING) {
        currentRandoms.push_back(randTriple.mask + ":" + std::to_string(randTriple.result) + ":" + std::to_string(randTriple.range));
    }
}

void RandomLogGenerator::addScreen(json screen) {
    if (logMode != NO_LOGGING) {
        roundScreens[currentSpin - 1].push_back(screen);
    }
}

void RandomLogGenerator::addWinAmount(double winAmount) {
    if (logMode == LOGGING || logMode == REPLAY) {
        if (logTumbleWinsIndividually) {
            currentSpinTumbleWins.push_back(winAmount);
        }
        else {
            if (currentSpinTumbleWins.empty()) {
                currentSpinTumbleWins.push_back(winAmount);
            }
            else {
                currentSpinTumbleWins.back() += winAmount;
            }
        }
        currentSpinTotalWin += winAmount;
    }
}

void RandomLogGenerator::addMultipliers(std::vector<int>& multipliersUsed) {
    if (logMode == LOGGING) {
        roundMultipliers.push_back(multipliersUsed);
    }
}

void RandomLogGenerator::addWheelBonusPrizes(std::vector<double>& wheelBonusPrizes) {
    if (logMode == LOGGING && !wheelBonusPrizes.empty()) {
        roundWheelBonusPrizes.push_back(wheelBonusPrizes);
    }
    else {
        roundWheelBonusPrizes.push_back({});
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
            if (part.empty()) continue;
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