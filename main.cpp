// main.cpp  —  template runner (C++14)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <iomanip>

#include "RandomUtils.h"   // for LogMode, SimulationMode, RandomLogGenerator (your existing file)
#include "Stats.h"
#include "GameConfig.h"
#include "GameInstance.h"

// --------------------------------------------------------------------------------------
// 1) Quick toggles you can edit per run (config.json remains for game-specific info only)
// --------------------------------------------------------------------------------------
namespace SimDefaults {
    constexpr LogMode        LOG_MODE = NO_LOGGING;   // NO_LOGGING | LOGGING | REPLAY
    constexpr SimulationMode SIM_MODE = RANDOM_MODE;  // EXACT_MODE | RANDOM_MODE | PLAYER_MODE | CSV_MODE
    constexpr long long      SPINS = 1'000'000;    // total spins across all threads
    constexpr int            THREADS = 12;           // threads for RANDOM_MODE (forced to 1 if logging/replay)
    constexpr bool           ALLOW_CLI_OVERRIDE = true; // --spins N --threads T --log X --mode X
}

// These globals exist in your codebase; keep definitions here.
LogMode        logMode = SimDefaults::LOG_MODE;
SimulationMode simulationMode = SimDefaults::SIM_MODE;

// Small timer for wall-time measurement
struct Timer {
    std::chrono::high_resolution_clock::time_point t0;
    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    double stop() const {
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(t1 - t0).count();
    }
};

static void applyCliOverrides(int argc, char** argv, long long& spins, int& threads, LogMode& lm, SimulationMode& sm) {
    if (!SimDefaults::ALLOW_CLI_OVERRIDE) return;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--spins" || arg == "-s") && i + 1 < argc) {
            spins = std::stoll(argv[++i]);
        }
        else if ((arg == "--threads" || arg == "-t") && i + 1 < argc) {
            threads = std::stoi(argv[++i]);
        }
        else if (arg == "--log" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "NO_LOGGING") lm = NO_LOGGING;
            else if (v == "LOGGING")    lm = LOGGING;
            else if (v == "REPLAY")     lm = REPLAY;
            else std::cerr << "Unknown --log " << v << " (using default)\n";
        }
        else if (arg == "--mode" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "RANDOM_MODE") sm = RANDOM_MODE;
            else if (v == "PLAYER_MODE") sm = PLAYER_MODE;
            else if (v == "CSV_MODE")    sm = CSV_MODE;
            else std::cerr << "Unknown --mode " << v << " (using default)\n";
        }
    }
}

int main(int argc, char** argv) {
    Timer timer; timer.start();

    // ------------------------
    // 2) Read game config JSON
    // ------------------------
    std::shared_ptr<GameConfig> config;
    try {
        config = std::make_shared<GameConfig>("config.json");
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load config.json: " << e.what() << "\n";
        return 1;
    }

    // Game metadata for output file naming
    const auto gameInfo = config->getGameInfo(); // [gameName, RTP, modeLabel]
    const auto& rtpHeads = config->getRTPHeaders();
    SymbolStructure symbolStructure = config->parseSymbolStructure();
    const double costPerSpin = static_cast<double>(config->getCost());

    const std::string baseName = gameInfo[0] + "_RTP" + gameInfo[1] + "_" + gameInfo[2];
    const std::string outputFileName = baseName + "_output.txt";
    const std::string randomLogFileName = baseName + "_randomLog.txt";
    const std::string gameDetailsFileName = baseName + "_gameDetails.txt";
    const std::string gameSpecificStatsFileName = baseName + "_gameSpecificStats.txt";

    // -------------------------------
    // 3) Resolve sim toggles + output
    // -------------------------------
    long long numberOfSpins = SimDefaults::SPINS;
    int       numThreads = SimDefaults::THREADS;

    // from code defaults; allow CLI overrides
    logMode = SimDefaults::LOG_MODE;
    simulationMode = SimDefaults::SIM_MODE;
    applyCliOverrides(argc, argv, numberOfSpins, numThreads, logMode, simulationMode);

    // Logging init (forces single-thread if not NO_LOGGING)
    if (logMode != NO_LOGGING) numThreads = 1;
    const bool loggingOk = RandomLogGenerator::handleLoggingMode(logMode, randomLogFileName, gameDetailsFileName);
    if (!loggingOk && logMode != NO_LOGGING) {
        std::cerr << "Failed to initialize logging/replay files.\n";
        return 1;
    }

    std::ofstream out(outputFileName);
    if (!out) {
        std::cerr << "Failed to open output file: " << outputFileName << "\n";
        return 1;
    }

    // Single aggregated stats object
    Stats finalStats(symbolStructure, rtpHeads, costPerSpin);

    // ----------------------------------------------------
    // 4) Run the selected simulation mode (RANDOM_MODE now)
    // ----------------------------------------------------
    if (simulationMode == RANDOM_MODE) {
        // Split spins across threads (integer divide; remainder goes to first thread)
        const long long spinsPerThread = numberOfSpins / std::max(1, numThreads);
        const long long remainder = numberOfSpins - spinsPerThread * std::max(1, numThreads);

        std::vector<std::thread> workers;
        std::vector<std::shared_ptr<Stats>> perThreadStats;
        perThreadStats.reserve(std::max(1, numThreads));

        for (int i = 0; i < std::max(1, numThreads); ++i) {
            const long long spinsThisThread = spinsPerThread + (i == 0 ? remainder : 0);

            auto statsPtr = std::make_shared<Stats>(symbolStructure, rtpHeads, costPerSpin);
            statsPtr->setNumIterations(spinsThisThread);
            perThreadStats.emplace_back(statsPtr);

            workers.emplace_back([config, &symbolStructure, statsPtr, spinsThisThread]() {
                GameInstance instance(config, symbolStructure, *statsPtr);
                instance.playBaseGame(spinsThisThread);
                });
        }

        for (auto& th : workers) th.join();

        // Aggregate results
        for (const auto& s : perThreadStats) finalStats.aggregate(*s);
        finalStats.calculateStandardDeviations();

        // Output core data (+ optional game-specific writer if you set it elsewhere)
        finalStats.outputData(out, gameSpecificStatsFileName);
        finalStats.printFrequencyTables();

    }
    else if (simulationMode == CSV_MODE) {
        std::string userGameVersion;
        std::cout << "Enter the game version : ";
        std::getline(std::cin, userGameVersion);

        const long long defaultSpins = 1000000LL;
        std::string csvFileName = outputFileBase + "_simulation.csv";

        std::ostringstream csvData;
        csvData << "GAME NAME: " << gameInfo[0] << "\n";
        csvData << "GAME VERSION: " << userGameVersion << "\n\n";
        csvData << "RTP SIMULATION RESULTS\n\n";
        csvData << "PLAYER 1 RTP SIMULATION RESULTS\n";
        csvData << "SPINID,TOTAL STAKE,BALANCE,BASE GAME,FREE SPINS,TOTALWIN,TOTAL WINS,REELSET_ID\n";

        long long totalWager = 0;
        double balance = 500.0, totalWins = 0.0; // Starting balance

        for (long long i = 0; i < defaultSpins; ++i) {
            double spinWin = 0.0, freeSpinWin = 0.0, baseGameWin = 0.0, modCost = (costPerSpin / 100);
            int reelsetId = -1; // You'll need a real getter here


            Stats stats(symbolStructure, rtpHeaders, costPerSpin);
            GameInstance gameInstance(config, symbolStructure, stats);

            gameInstance.playBaseGame(1); // Simulate a single spin

            spinWin = stats.getLastSpinPayout() / 100;
            freeSpinWin = stats.getFreeSpinPayout() / 100;
            baseGameWin = spinWin - freeSpinWin;
            totalWins += spinWin;

            // Assuming GameInstance has a method to get reelset ID
            reelsetId = gameInstance.getLastReelSetID(); // <-- implement this if not present

            totalWager += modCost;
            balance += spinWin - modCost;

            csvData << i << ',' << (i + 1) * modCost << ',' << std::fixed << std::setprecision(2) << balance << ','
                << baseGameWin << ',' << freeSpinWin << ',' << spinWin << ',' << totalWins << ',' << reelsetId << '\n';
        }

        std::ofstream csvFile(csvFileName);
        if (!csvFile.is_open()) {
            std::cerr << "Failed to open CSV output file: " << csvFileName << std::endl;
            return 1;
        }
        csvFile << csvData.str();
        csvFile.close();

        std::cout << "CSV simulation completed. Output file: " << csvFileName << std::endl;
    }
    else if (simulationMode == PLAYER_MODE) {
        int N = 10000; // Number of players 100000
        int X = 2000; // Starting credits (enough for 100 spins)
        int Y = 4000; // Target credits (enough for 200 spins)
        int successfulPlayers = 0;

        for (int i = 0; i < N; ++i) {
            // Initialize Stats for each player (if stats aggregation is not needed across players)
            Stats stats(symbolStructure, rtpHeaders, costPerSpin);

            // Initialize PlayerSimulation with the shared config, symbolStructure, and unique Stats instance
            PlayerSimulation sim(X, Y, config, symbolStructure, stats);

            if (sim.simulate()) {
                successfulPlayers++;
            }
        }

        double successPercentage = (double)successfulPlayers / N * 100.0;
        cout << "Percentage of players reaching target credits: " << successPercentage << "%\n";
        return 0;
    }
    else {
        std::cerr << "Unknown SimulationMode.\n";
        out << "Unknown SimulationMode.\n";
        out.close();
        RandomLogGenerator::closeLogs();
        return 1;
    }

    // -----------------------
    // 5) Footer + clean close
    // -----------------------
    const double elapsed = timer.stop();
    out << "\nElapsed time: " << std::fixed << std::setprecision(3) << elapsed << " s\n";
    out.close();

    RandomLogGenerator::closeLogs();
    return 0;
}
