#pragma once
#include "GameConfig.h" // Include GameConfig to access configuration data
#include <vector>
#include <algorithm>
#include <fstream>
#include <cmath> // Include for std::sqrt
#include <unordered_map>
#include <mutex>
#include <string>
#include <iomanip>
#include <iostream>

#include <utility> // For std::pair
#include <functional> // For hash specialization

namespace std {
	template <>
	struct hash<std::pair<int, int>> {
		size_t operator()(const std::pair<int, int>& p) const {
			return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
		}
	};
}

class Stats {
private:
        std::mutex statsMutex;
        std::function<void(const Stats&, std::ostream&)> gameSpecificWriter_;

	long long numIterations;
	long long baseGameHits = 0;
	double costPerSpin;
	double totalWin;
	std::vector<std::string> rtpHeaders;
	std::vector<double> payVector, lastPay;
	std::vector<std::unordered_map<double, long long>> payFrequencies;
	std::unordered_map<std::string, long long> featureHits;
	std::vector<std::vector<long long>> baseSymHits;
	std::vector<std::vector<double>> baseSymPays;
	std::unordered_map<int, long long> scatterHits, freeSpinsFreq, tumbleFreq, multFreq, multFreqFree;
	std::unordered_map<int, std::unordered_map<int, long long>> multFreqFreeByInit;
	SymbolStructure& symbolStructure;
	std::vector<double> standardDeviations;
	int totalWins = 0;
	double totalWinnings = 0.0;
	std::pair<int, double> moneyEntry; // <count, amount>

        std::unordered_map<std::pair<int, int>, long long, std::hash<std::pair<int, int>>> scaleFrequency;

        template <typename Map, typename Key>
        void incrementCounter(Map& container, const Key& key) {
                std::lock_guard<std::mutex> lock(statsMutex);
                ++container[key];
        }

        template <typename Map, typename OuterKey, typename InnerKey>
        void incrementNestedCounter(Map& container, const OuterKey& outer, const InnerKey& inner) {
                std::lock_guard<std::mutex> lock(statsMutex);
                ++container[outer][inner];
        }

        void writeDefaultStats(std::ostream& file) const {
                file << "RTP and Standard Deviation Breakdown\n";
                file << "Name\tRTP\tStDev\n";

                for (size_t i = 0; i < rtpHeaders.size(); ++i) {
                        double rtp = payVector[i] / (numIterations * costPerSpin);
                        double stDev = standardDeviations[i];
                        file << rtpHeaders[i] << '\t' << std::setprecision(6) << rtp << '\t' << std::setprecision(4) << stDev << '\n';
                }
                file << "----------------------------------------\n";

                file << "Iterations\t" << numIterations << '\n';
                file << "Total Pay\t" << payVector[3] << '\n';

                file << "Feature Hits\n";

                file << "Feature\tHits\tHit Rate\n";

                std::vector<std::pair<std::string, long long>> sortedFeatures(featureHits.begin(), featureHits.end());

                std::sort(sortedFeatures.begin(), sortedFeatures.end(),
                        [](const auto& a, const auto& b) {
                                return a.second > b.second;
                        });

                for (const auto& pair : sortedFeatures) {
                        long long hits = pair.second;
                        double hitRate = (hits > 0) ? numIterations / static_cast<double>(hits) : 0.0;
                        file << pair.first << '\t' << hits << '\t' << std::setprecision(8) << hitRate << '\n';
                }

                file << "----------------------------------------\n";

                file << "Base Hits\n";
                file << "Symbol";
                for (size_t i = 0; i < baseSymHits[0].size(); ++i) {
                        file << '\t' << i + 1;
                }
                file << '\n';
                for (size_t i = 0; i < baseSymHits.size(); ++i) {
                        file << symbolStructure.getSymbols()[i];
                        for (const auto& hits : baseSymHits[i]) {
                                file << '\t' << hits;
                        }
                        file << '\n';
                }

                file << "----------------------------------------\n";

                file << "Base Pays\n";
                file << "Symbol";
                for (size_t i = 0; i < baseSymPays[0].size(); ++i) {
                        file << '\t' << i + 1;
                }
                file << '\n';
                for (size_t i = 0; i < baseSymPays.size(); ++i) {
                        file << symbolStructure.getSymbols()[i];
                        for (const auto& hits : baseSymPays[i]) {
                                file << '\t' << hits;
                        }
                        file << '\n';
                }
                file << "----------------------------------------\n";
                file << "Average Free Spins: " << '\t' << calculateAverageFrequency(freeSpinsFreq) << '\n';
                file << "----------------------------------------\n";
        }

        void writeGameSpecificStats(std::ostream& file) const {
                file << "Average Tumbles: " << '\t' << calculateAverageFrequency(tumbleFreq) << '\n';
                file << "----------------------------------------\n";
                file << "Tumble Frequencies\n";
                file << "Number Tumble\tFrequency\n";
                for (const auto& pair : tumbleFreq) {
                        file << pair.first << '\t' << pair.second << '\n';
                }
                file << "----------------------------------------\n";

                file << "Average Final Multiplier: " << '\t' << calculateAverageFrequency(multFreq) << '\n';
                file << "Final Multiplier Frequencies\n";
                file << "Multiplier\tFrequency\n";
                for (const auto& pair : multFreq) {
                        file << pair.first << '\t' << pair.second << '\n';
                }
                file << "----------------------------------------\n";
                file << "Average Final Multiplier Free Spins: " << '\t' << calculateAverageFrequency(multFreqFree) << '\n';
                file << "Final Multiplier Frequencies Free Spins\n";
                file << "Multiplier\tFrequency\n";
                for (const auto& pair : multFreqFree) {
                        file << pair.first << '\t' << pair.second << '\n';
                }
                file << "----------------------------------------\n";
                file << "Final Multiplier Frequencies Free Spins (split by initial multiplier)\n";

                std::vector<int> initKeys;
                initKeys.reserve(multFreqFreeByInit.size());
                for (const auto& kv : multFreqFreeByInit) initKeys.push_back(kv.first);
                std::sort(initKeys.begin(), initKeys.end());

                for (int init : initKeys) {
                        const auto& freq = multFreqFreeByInit.at(init);

                        double avg = calculateAverageFrequency(const_cast<std::unordered_map<int, long long>&>(freq));

                        file << "Init Multiplier: " << init << "\n";
                        file << "Average Final Multiplier (init " << init << "):\t" << avg << "\n";
                        file << "Final Mult\tFrequency\n";

                        std::vector<std::pair<int, long long>> rows(freq.begin(), freq.end());
                        std::sort(rows.begin(), rows.end(),
                                [](auto& a, auto& b) { return a.first < b.first; });

                        for (const auto& p : rows) {
                                file << p.first << '\t' << p.second << '\n';
                        }
                        file << "----------------------------------------\n";
                }

                if (gameSpecificWriter_) {
                        printSectionHeader(file, "Game-Specific Report");
                        gameSpecificWriter_(*this, file);
                }
        }

public:
	explicit Stats(SymbolStructure& symbolStructure, const std::vector<std::string>& rtpHeaders, double costPerSpin)
		: symbolStructure(symbolStructure), rtpHeaders(rtpHeaders), costPerSpin(costPerSpin) {
		numIterations = 0;
		totalWin = 0.0;

		size_t numRTPs = rtpHeaders.size();
		payVector.resize(numRTPs, 0.0);
		payFrequencies.resize(numRTPs);

		// featureHits.resize(featureNames.size(), 0);

		int numSymbols = symbolStructure.getNumSymbols();
		int maxLength = symbolStructure.getWinLength();
		baseSymHits.resize(numSymbols, std::vector<long long>(maxLength, 0));
		baseSymPays.resize(numSymbols, std::vector<double>(maxLength, 0.0));
	}
        // Set a custom writer for game-specific reporting (optional).
        void setGameSpecificWriter(std::function<void(const Stats&, std::ostream&)> fn) {
                gameSpecificWriter_ = std::move(fn);
        }

        void printSectionHeader(std::ostream& out, const std::string& title) const {
                out << "\n==== " << title << " ====\n";
        }

        void outputDefaultStats(std::ostream& out) const {
                writeDefaultStats(out);
        }

        void outputGameSpecificStats(std::ostream& out) const {
                writeGameSpecificStats(out);
        }

	void setNumIterations(long long iterations) {
		std::lock_guard<std::mutex> lock(statsMutex);
		numIterations = iterations;
	}

	void trackResult(const std::string& symbol, int length, int ways, double pay, bool base) {
		std::lock_guard<std::mutex> lock(statsMutex);
		int symbolIndex = symbolStructure.findSymbolIndex(symbol);
		int lengthIndex = length - 1;
		if (base) {
			baseSymHits[symbolIndex][lengthIndex] += ways;
			baseSymPays[symbolIndex][lengthIndex] += pay;
		}
	}

        void recordScatterHit(int prize) {
                incrementCounter(scatterHits, prize);
        }

        void recordTumbleFrequency(int tumbles) {
                incrementCounter(tumbleFreq, tumbles);
        }

        void recordFinalMult(int mult) {
                incrementCounter(multFreq, mult);
        }

        void recordFinalMultFree(int mult) {
                incrementCounter(multFreqFree, mult);
        }

        void recordFinalMultFreeByInit(int initMult, int finalMult) {
                incrementNestedCounter(multFreqFreeByInit, initMult, finalMult);
        }

        //record number of free spins
        void recordFreeSpins(int freeSpins) {
                incrementCounter(freeSpinsFreq, freeSpins);
        }

	//double calculateAverageTumbleFrequency() const {
	//	long long totalTumbles = 0;
	//	long long totalOccurrences = 0;
	//	for (const auto& pair : tumbleFreq) {
	//		totalTumbles += pair.first * pair.second;
	//		totalOccurrences += pair.second;
	//	}
	//	if (totalOccurrences == 0) {
	//		return 0.0;
	//	}
	//	return static_cast<double>(totalTumbles) / totalOccurrences;
	//}

        double calculateAverageFrequency(const std::unordered_map<int, long long>& freqMap) const {
                long long totalHits = 0;
                long long totalOccurrences = 0;
                for (const auto& pair : freqMap) {
                        totalHits += pair.first * pair.second;
                        totalOccurrences += pair.second;
		}
		if (totalOccurrences == 0) {
			return 0.0;
		}
		return static_cast<double>(totalHits) / totalOccurrences;
	}

	/*double calculateAverageFreeSpins() const {
		long long totalFreeSpins = 0;
		long long totalOccurrences = 0;
		for (const auto& pair : freeSpinsFreq) {
			totalFreeSpins += pair.first * pair.second;
			totalOccurrences += pair.second;
		}
		if (totalOccurrences == 0) {
			return 0.0;
		}
		return static_cast<double>(totalFreeSpins) / totalOccurrences;
	}*/

	void completeWager(const std::vector<double>& pays) {
		std::lock_guard<std::mutex> lock(statsMutex);
		for (size_t i = 0; i < pays.size(); i++) {
			payVector[i] += pays[i];
			payFrequencies[i][pays[i]]++;
		}
		if (pays[0] > 0) {
			baseGameHits++;
		}
		lastPay = pays;
	}

	//void trackFeatureActivation(const std::string& featureName) {
	//    std::lock_guard<std::mutex> lock(statsMutex);
	//    auto it = std::find(featureNames.begin(), featureNames.end(), featureName);
	//    if (it != featureNames.end()) {
	//        size_t index = std::distance(featureNames.begin(), it);
	//        featureHits[index]++;
	//    }
	//}

        void trackFeatureActivation(const std::string& featureName) {
                incrementCounter(featureHits, featureName);
        }

	double calculateStandardDeviation(const std::vector<double>& pays) const {
		if (pays.empty()) return 0.0;
		double mean = std::accumulate(pays.begin(), pays.end(), 0.0) / pays.size();
		double variance = 0.0;
		for (auto pay : pays) {
			variance += std::pow(pay - mean, 2);
		}
		variance /= pays.size();
		return std::sqrt(variance);
	}

	void calculateStandardDeviations() {
		std::lock_guard<std::mutex> lock(statsMutex);
		standardDeviations.clear();
		standardDeviations.resize(payFrequencies.size(), 0.0);

		for (size_t i = 0; i < payFrequencies.size(); ++i) {
			double mean = 0.0;
			double variance = 0.0;
			double totalWeight = 0.0;

			for (const auto& pair : payFrequencies[i]) {
				mean += pair.first * pair.second;
				totalWeight += pair.second;
			}
			mean /= totalWeight;

			for (const auto& pair : payFrequencies[i]) {
				variance += pair.second * std::pow(pair.first - mean, 2);
			}
			variance /= totalWeight;

			standardDeviations[i] = std::sqrt(variance);
		}
	}

	void aggregate(const Stats& other) {
		std::lock_guard<std::mutex> lock(statsMutex);

		numIterations += other.numIterations;
		totalWin += other.totalWin;
		baseGameHits += other.baseGameHits;

		for (size_t i = 0; i < payVector.size(); ++i) {
			payVector[i] += other.payVector[i];
			for (const auto& freqPair : other.payFrequencies[i]) {
				payFrequencies[i][freqPair.first] += freqPair.second;
			}
		}

		// Aggregate featureHits
		for (const auto& pair : other.featureHits) {
			featureHits[pair.first] += pair.second;
		}

		for (size_t i = 0; i < baseSymHits.size(); ++i) {
			for (size_t j = 0; j < baseSymHits[i].size(); ++j) {
				baseSymHits[i][j] += other.baseSymHits[i][j];
				baseSymPays[i][j] += other.baseSymPays[i][j];
			}
		}

		for (const auto& pair : other.scatterHits) {
			scatterHits[pair.first] += pair.second;
		}

		for (const auto& pair : other.tumbleFreq) {
			tumbleFreq[pair.first] += pair.second;
		}

		for (const auto& pair : other.multFreq) {
			multFreq[pair.first] += pair.second;
		}
		for (const auto& pair : other.multFreqFree) {
			multFreqFree[pair.first] += pair.second;
		}
		for (const auto& outerPair : other.multFreqFreeByInit) {
			int initMult = outerPair.first;
			for (const auto& innerPair : outerPair.second) {
				multFreqFreeByInit[initMult][innerPair.first] += innerPair.second;
			}
		}

		for (const auto& pair : other.freeSpinsFreq) {
			freeSpinsFreq[pair.first] += pair.second;
		}

		for (const auto& pair : other.scaleFrequency) {
			scaleFrequency[pair.first] += pair.second;
		}
		moneyEntry.first += other.moneyEntry.first;
		moneyEntry.second += other.moneyEntry.second;


		totalWins += other.totalWins;
		totalWinnings += other.totalWinnings;
	}

	double getLastSpinPayout() const {
		if (lastPay.empty()) return 0.0;
		return lastPay.back();
	}

	void recordWin(double amount) {
		std::lock_guard<std::mutex> lock(statsMutex);
		totalWins++;
		totalWinnings += amount;
	}

        void outputData(std::ostream& defaultStream, const std::string& gameSpecificFilename) const {
                outputDefaultStats(defaultStream);

                std::ofstream gameSpecificFile(gameSpecificFilename);
                if (!gameSpecificFile.is_open()) {
                        std::cerr << "Failed to open game-specific stats file: " << gameSpecificFilename << std::endl;
                        return;
                }

                outputGameSpecificStats(gameSpecificFile);
        }

	void printFrequencyTableToFile(const std::string& categoryName, const std::unordered_map<double, long long>& frequencyMap) const {
		std::string filename = "pay_frequency_" + categoryName + ".txt";
		std::ofstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to open " << filename << std::endl;
			return;
		}

		std::vector<std::pair<double, long long>> freqVector(frequencyMap.begin(), frequencyMap.end());
		std::sort(freqVector.begin(), freqVector.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		file << "Pay\tFrequency\n";
		for (const auto& pair : freqVector) {
			file << pair.first << "\t" << pair.second << "\n";
		}

		file.close();
	}

	void printFrequencyTables() const {
		for (size_t i = 0; i < payFrequencies.size(); ++i) {
			printFrequencyTableToFile(rtpHeaders[i], payFrequencies[i]);
		}
	}

	int getTumbleCount() const {
		return std::accumulate(tumbleFreq.begin(), tumbleFreq.end(), 0,
			[](int sum, const auto& pair) { return sum + pair.second; });
	}

        double getFreeSpinPayout() const {
                if (lastPay.size() <= 3) {
                        return 0.0;
                }
                return lastPay[3];
        }


	void trackMoneyEntry(double amount) {
		std::lock_guard<std::mutex> lock(statsMutex);
		moneyEntry.first++;
		moneyEntry.second += amount;
	}

};
