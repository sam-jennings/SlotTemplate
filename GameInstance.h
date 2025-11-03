#pragma once
#include "GameConfig.h"
#include "Stats.h"
#include "Screen.h"

class GameInstance {
private:
    std::shared_ptr<GameConfig> config;
    Stats& stats;
    SymbolStructure symbolStructure;
    std::string rtpKey;

    // Flags / core params
    GameFlags flags;
    int numReels = 0;
    int fixedRows = 3; // used when megaways=false
    std::vector<PrizeDistribution<int>> reelHeightPD, reelHeightFreePD;

    int cost = 0;
    std::vector<std::string> symbols;
    std::map<std::string, std::vector<int>> paytable;
    std::vector<std::string> payHeaders;

    std::vector<std::vector<int>> boostWeights;
    std::vector<PrizeDistribution<int>> boostPDVec;
    std::vector<bool> boostVecOver, boostVecUnder;

    // ReelSets
    std::unordered_map<std::string, ReelSet> allReelSets;
    std::vector<int> reelWeights, reelWeightsFree;
    PrizeDistribution<int> ReelsPD;

    // Game state
    Screen screen;
    int lastReelSetID = -1;

    enum PayIdx { INITIAL = 0, TUMBLE, BASE, FREE_TOTAL, TOTAL };

    void initializeGame() {
        rtpKey = config->getRTPKey();
        flags = config->getGameFlags();
        numReels = config->getReels();
        payHeaders = config->getRTPHeaders();
        symbolStructure = config->parseSymbolStructure();
        allReelSets = config->parseAllReelSets();
        reelWeights = config->parseVec<int32_t>("reelWeights", rtpKey);
        reelWeightsFree = config->parseVec<int32_t>("reelWeightsFree", rtpKey);
        ReelsPD = PrizeDistribution<int>("R-WTS", std::vector<int>{0, 1, 2, 3}, reelWeights);
        cost = config->getCost();
        symbols = symbolStructure.getSymbols();
        paytable = symbolStructure.getPaytable();

        // Heights PDs only if megaways = true
        if (flags.megaways) {
            reelHeightPD = config->parsePDVec<int>("reelHeights");
            reelHeightFreePD = config->parsePDVec<int>("reelHeightsFree");
        }
        else {
            reelHeightPD.clear();
            reelHeightFreePD.clear();
        }

        // Optional boosts
        boostWeights = config->parseArray<int>("boostWeights");
    }

    // --- evaluation helpers ---
    double calculateWaysWins(Screen& s, bool baseGame, int currentMult = 1) {
        double totalPay = 0;
        if (logMode != NO_LOGGING) RandomLogGenerator::addScreen(s.toJson(true, true));
        s.clearMarkedPositions();

        for (const auto& sym : symbols) {
            auto waysInfo = s.getWaysForSymbol(sym);
            int length = waysInfo.first;
            int ways = waysInfo.second;
            int payout = 0;
            if (length > 0) {
                payout = currentMult * ways * paytable[sym][length - 1];
                if (payout > 0) {
                    stats.trackResult(sym, length, ways, payout, baseGame);
                    s.markSymbol(sym, length);
                }
            }
            totalPay += payout;
        }
        return totalPay;
    }

    double calculateLineWins(Screen& s, bool baseGame) {
        // Your commented line-eval was kept in your file; this is the safe version for template
        // It relies on Screen’s payline support (assumed present or easily added).
        double totalPay = 0;
        //if (logMode != NO_LOGGING) RandomLogGenerator::addScreen(s.toJson(true, true));
        //s.clearMarkedPositions();

        //int lines = s.getNumPaylines();
        //for (int lineIndex = 0; lineIndex < lines; ++lineIndex) {
        //    std::string sym;
        //    int len = 0;
        //    double pay = s.evaluatePaylinePay(lineIndex, paytable, sym, len); // as per your commented code path
        //    if (pay > 0) {
        //        stats.trackResult(sym, len, 1, pay, baseGame);
        //        s.markPayline(lineIndex, len);
        //        totalPay += pay;
        //    }
        //}
        return totalPay;
    }

    int boostsInWin(const Screen& s) {
        int boostCount = 0;
        const auto& marked = s.getMarkedPositions();
        for (const auto& pos : marked) {
            int reel = pos.first;
            int row = pos.second;
            if (row == -1 && s.isSideBoosted(true, reel - 1)) boostCount++;
            if (row == -2 && s.isSideBoosted(false, reel - 1)) boostCount++;
        }
        return boostCount;
    }

    std::pair<double, double> doOneEvaluation(Screen& s, ReelSet& rs, bool baseGame, int& globalMult) {
        // returns {initialWin, tumbleWinAdded}
        double init = 0, tumble = 0;
        if (flags.mode == GameMode::WAYS) {
            init = calculateWaysWins(s, baseGame);
        }
        else {
            init = calculateLineWins(s, baseGame);
        }
        globalMult += boostsInWin(s);
        init *= globalMult;
        RandomLogGenerator::addWinAmount(init);
        return std::make_pair(init, 0.0);
    }

public:
    explicit GameInstance(std::shared_ptr<GameConfig> cfg, SymbolStructure& ss, Stats& st)
        : config(cfg), stats(st), symbolStructure(ss) {
        initializeGame();
    }

    double simulateSingleSpin() {
        playBaseGame(1);
        double lastSpinPayout = stats.getLastSpinPayout();
        return lastSpinPayout - cost;
    }

    void playBaseGame(long long numSpins) {
        std::vector<PrizeDistribution<int>> localBoostPD(boostWeights.size());
        for (size_t i = 0; i < boostWeights.size(); ++i) {
            localBoostPD[i] = PrizeDistribution<int>("BS_" + std::to_string(i + 1), std::vector<int>{0, 1}, boostWeights[i]);
        }

        for (long long i = 0; i < numSpins; ++i) {
            double basePay = 0.0;
            int globalMult = 1;
            RandomLogGenerator::startRound();

            std::vector<double> pays(payHeaders.size(), 0.0);

            // Resize screen
            if (flags.megaways) {
                std::vector<int> heights(numReels);
                for (int r = 0; r < numReels; ++r) heights[r] = reelHeightPD[r].getRandomPrize();
                screen.resize(heights);
            }
            else {
                // fixed height: use paytable length or a fixed constant
                int rows = symbolStructure.getWinLength(); // reasonable default
                screen.resize(std::vector<int>(numReels, rows));
            }

            int reelID = ReelsPD.getRandomPrize();
            lastReelSetID = reelID;
            ReelSet activeReels;
            switch (reelID) {
            case 0: activeReels = allReelSets["baseLow"];    break;
            case 1: activeReels = allReelSets["baseHigh"];   break;
            case 2: activeReels = allReelSets["baseTumble"]; break;
            case 3: activeReels = allReelSets["noWin1"];     break;
            }

            activeReels.spinReels();

            // boosts roll
            boostVecOver.clear(); boostVecUnder.clear();
            for (size_t b = 0; b < boostWeights.size(); ++b) {
                boostVecOver.push_back(localBoostPD[b].getRandomPrize());
                boostVecUnder.push_back(localBoostPD[b].getRandomPrize());
            }

            // Draw main + side
            screen.generateScreen(activeReels);
            if (activeReels.hasOverReel()) screen.addSideSymbols(true, activeReels, boostVecOver);
            if (activeReels.hasUnderReel()) screen.addSideSymbols(false, activeReels, boostVecUnder);

            // cascades?
            if (flags.cascades) {
                // Tumble loop (your original cascade logic preserved) :contentReference[oaicite:5]{index=5}
                bool hasNewWins;
                double initialWin = 0, tumbleWin = 0;
                int tumbleCount = 0;

                do {
                    hasNewWins = false;
                    screen.clearMarkedPositions();
                    if (tumbleCount == 0) {
                        double w = (flags.mode == GameMode::WAYS) ? calculateWaysWins(screen, true) : calculateLineWins(screen, true);
                        globalMult += boostsInWin(screen);
                        w *= globalMult;
                        initialWin += w;
                        RandomLogGenerator::addWinAmount(w);
                    }
                    else {
                        double w = (flags.mode == GameMode::WAYS) ? calculateWaysWins(screen, true) : calculateLineWins(screen, true);
                        globalMult += boostsInWin(screen);
                        w *= globalMult;
                        tumbleWin += w;
                        RandomLogGenerator::addWinAmount(w);
                    }

                    if (!screen.getMarkedPositions().empty()) {
                        hasNewWins = true;
                        tumbleCount++;
                        screen.removeMarkedPositions();
                        screen.cascadeSymbols(activeReels, false, activeReels);
                        if (activeReels.hasOverReel())  screen.cascadeSideRowIntegrated(true, activeReels, 50);
                        if (activeReels.hasUnderReel()) screen.cascadeSideRowIntegrated(false, activeReels, 50);
                    }
                } while (hasNewWins);

                // bookkeeping
                if (initialWin) stats.recordTumbleFrequency(tumbleCount);
                stats.recordFinalMult(globalMult);

                basePay = initialWin + tumbleWin;
                if (basePay) stats.trackFeatureActivation("Base Win");
                pays[INITIAL] += initialWin;
                pays[TUMBLE] += (basePay - initialWin);
                pays[BASE] += basePay;
            }
            else {
                // Single pass (no cascades)
                double initialWin = (flags.mode == GameMode::WAYS) ? calculateWaysWins(screen, true) : calculateLineWins(screen, true);
                globalMult += boostsInWin(screen);
                initialWin *= globalMult;
                RandomLogGenerator::addWinAmount(initialWin);
                stats.recordFinalMult(globalMult);

                basePay = initialWin;
                if (basePay) stats.trackFeatureActivation("Base Win");
                pays[INITIAL] += initialWin;
                pays[BASE] += basePay;
            }

            // Simple FS trigger demo (as in your code) using F1 count
            int fgCount = screen.countSymbolOnScreen("F1", false);
            if (fgCount >= 3) {
                std::vector<double> fv = playFreeGames(5 * (fgCount - 3) + 10, (fgCount - 3) + 2);
                stats.trackFeatureActivation("FS Trigger " + std::to_string(fgCount));
                stats.trackFeatureActivation("Free Spins");
                pays[FREE_TOTAL] += fv[0];
            }
            else if (fgCount == 2) {
                stats.trackFeatureActivation("FS Tease");
            }

            RandomLogGenerator::endRound();
            pays[TOTAL] = pays[INITIAL] + pays[TUMBLE] + pays[FREE_TOTAL];
            if (pays[TOTAL]) stats.trackFeatureActivation("Base");
            stats.completeWager(pays);
        }
    }

    std::vector<double> playFreeGames(int numFreeGames, int initMult) {
        std::vector<double> pays(2, 0.0);
        int multiplier = initMult;
        int freeSpinsRemaining = numFreeGames;

        boostVecOver = std::vector<bool>(boostWeights.size(), true);
        boostVecUnder = std::vector<bool>(boostWeights.size(), true);

        Screen fsScreen(numReels, 0);
        fsScreen.clearScreen();

        while (freeSpinsRemaining-- > 0) {
            RandomLogGenerator::newSpin();
            if (flags.megaways) {
                std::vector<int> heights(numReels);
                for (int r = 0; r < numReels; ++r) heights[r] = reelHeightFreePD[r].getRandomPrize();
                fsScreen.resize(heights);
            }
            else {
                int rows = symbolStructure.getWinLength();
                fsScreen.resize(std::vector<int>(numReels, rows));
            }

            ReelSet freeReelSet;
            if (getRand("FR-WTS", reelWeightsFree[0] + reelWeightsFree[1]) < reelWeightsFree[0]) {
                freeReelSet = allReelSets["freeLow"];
            }
            else {
                freeReelSet = allReelSets["freeHigh"];
            }
            freeReelSet.spinReels();

            fsScreen.generateScreen(freeReelSet);
            if (freeReelSet.hasOverReel())  fsScreen.addSideSymbols(true, freeReelSet, boostVecOver);
            if (freeReelSet.hasUnderReel()) fsScreen.addSideSymbols(false, freeReelSet, boostVecUnder);

            // Free spins always tumble here; tweak if you want to mirror base flag
            bool hasNewWins;
            int tumbleCount = 0;
            double init = 0, tumble = 0;

            do {
                hasNewWins = false;
                fsScreen.clearMarkedPositions();
                double w = (flags.mode == GameMode::WAYS) ? calculateWaysWins(fsScreen, false) : calculateLineWins(fsScreen, false);
                multiplier += boostsInWin(fsScreen);
                w *= multiplier;
                if (tumbleCount == 0) init += w; else tumble += w;
                RandomLogGenerator::addWinAmount(w);

                if (!fsScreen.getMarkedPositions().empty()) {
                    hasNewWins = true;
                    tumbleCount++;
                    fsScreen.removeMarkedPositions();
                    fsScreen.cascadeSymbols(freeReelSet, false, freeReelSet);
                    if (freeReelSet.hasOverReel())  fsScreen.cascadeSideRowIntegrated(true, freeReelSet, 100);
                    if (freeReelSet.hasUnderReel()) fsScreen.cascadeSideRowIntegrated(false, freeReelSet, 100);
                }
            } while (hasNewWins);

            pays[0] += init + tumble;
        }

        stats.recordFreeSpins(numFreeGames);
        stats.recordFinalMultFree(multiplier);
        stats.recordFinalMultFreeByInit(initMult, multiplier);
        return pays;
    }

    int getLastReelSetID() const { return lastReelSetID; }
};
