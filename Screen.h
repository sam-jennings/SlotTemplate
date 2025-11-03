#pragma once

#include <string>
#include <vector>
#include <iostream>
#include "RandomLogGenerator.h"
#include "Symbols.h"



using namespace std;
using json = nlohmann::json;

struct SideCell {
    std::string name;
    bool boosted = false;
};

class Screen {
private:
    int numReels;
    int maxHeight;
    std::vector<int> heights;
    vector<vector<std::string>> grid;
	// For over/under reels
    static constexpr int SIDE_LEN = 4;          // middle-four reels
    //std::array<std::string, SIDE_LEN> overRow{}; // index 0 ⟶ reel 1, 3 ⟶ reel 4
    //std::array<std::string, SIDE_LEN> underRow{}; 
    std::array<SideCell, SIDE_LEN> overRow{};  
    std::array<SideCell, SIDE_LEN> underRow{};

public:
    std::vector<std::pair<int, int>> markedPositions;

    // Default constructor
    Screen() {}

    // Constructor for screen with equal rows
    Screen(int _numReels, int _numRows) : numReels(_numReels), maxHeight(_numRows) {
        heights.reserve(_numReels);
        for (int i = 0; i < _numReels; ++i) {
			heights.push_back(_numRows); // Initialize all reels with the same height
		}
        // Initialize the grid with placeholders
        resize(heights);
    }

    // Constructor for screen with variable heights
    Screen(const std::vector<int>& _heights) : numReels(_heights.size()), heights(_heights) {
        resize(heights);
	}


    // For over/under reels
    inline bool middleReel(int reel) const { return reel >= 1 && reel <= 4; }

    inline bool match(const std::string& symbol, const std::string& target, bool includeWild = true) const {
        if (includeWild && (symbol == target || symbol == "WL")) return true;
        return symbol == target;
	}

    void setSideSymbol(bool over, int idx, const std::string& s, bool boosted = false) {
        auto& cell = (over ? overRow : underRow)[idx];
        cell.name = s;
        cell.boosted = boosted;
    }

    std::string getSideSymbol(bool over, int idx) const {
        return (over ? overRow : underRow)[idx].name;
    }

    // Optional explicit boost accessors if you want them:
    bool isSideBoosted(bool over, int idx) const {
        return (over ? overRow : underRow)[idx].boosted;
    }
    void setSideBoosted(bool over, int idx, bool b) {
        (over ? overRow : underRow)[idx].boosted = b;
    }


    // add symbols to over/under reels from ReelSet
 /*   void addSideSymbols(bool over, const ReelSet& rs, std::vector<bool> boostVec = { 0,0,0,0 }) {
        const auto& strip = rs.reels[0].symbols;
        for (int i = 0; i < SIDE_LEN; ++i)
            setSideSymbol(over, i, strip[(rs.currentIndices[0] + i) % strip.size()], boostVec[i]);
    }
*/

    // Resize the screen based on fixed number of rows
    void resize(int _numReels, int _numRows) {
		numReels = _numReels;
		maxHeight = _numRows;
		heights.resize(numReels, _numRows); // Initialize all reels with the same height
		grid.resize(numReels, vector<string>(_numRows, "")); // Initialize the grid with empty strings
	}

    // Resize the screen with variable heights
    void resize(std::vector<int> newH) {
		heights = newH; // Update the heights vector with the new heights
		numReels = heights.size(); // Update the number of reels based on the new heights
        grid.resize(numReels);
        maxHeight = 0;
        // Resize each inner vector to have _numRows elements
        for (int i = 0; i < numReels; ++i) {
            if (heights[i] > maxHeight) {
				maxHeight = heights[i]; // Update maxHeight if the current reel's height is greater
			}
            grid[i].resize(heights[i], ""); // Ensure all new elements are initialized to empty strings
        }
    }

    void setReelHeight(int r, int h) { heights[r] = h; grid[r].resize(h); }

    int getReelHeight(int r) const { return heights[r]; }


    void display(bool displayMarkedPositions = false) {
        cout << "Current Screen:" << endl;
        for (int i = 0; i < maxHeight; ++i) {
            for (int j = 0; j < numReels; ++j) {
                if (i >= heights[j]) {
					cout << setw(5) << "     "; // Print empty space for reels that are shorter than the current row
					continue;
				}
                if (displayMarkedPositions) {
                    bool marked = false;
                    for (const auto& pos : markedPositions) {
                        if (pos.first == j && pos.second == i) {
                            marked = true;
                            break;
                        }
                    }
                    if (marked) {
                       cout << setw(5) << "[" << grid[j][i] << "] ";
//cout << setw(5) << ("[" + grid[j][i] + "]");
                    }
                    else {
                        cout << setw(5) << grid[j][i] << "  ";
                    }
                }
                else {
                    cout << setw(5) << grid[j][i] << "  ";
                }
            }
            cout << endl;
        }
    }

    // Function to update a cell in the screen with a new symbol
    void updateCell(int reel, int row, const std::string& symbol) {
        if (row >= 0 && row < heights[reel] && reel >= 0 && reel < numReels) {
            grid[reel][row] = symbol;
        }
    }

    // Function to clear the screen
    void clearScreen() {
        for (int i = 0; i < numReels; ++i) {
            for (int j = 0; j < heights[i]; ++j) {
                grid[i][j] = "";
            }
        }
    }

    //// Function to fill the screen with symbols from spinning reel sets
    //void fillScreen(const vector<vector<string>>& spinResults) {
    //    // Clear the screen
    //    clearScreen();

    //    // Fill the screen with symbols from spinResults
    //    for (int i = 0; i < min(numReels, (int)spinResults[i].size()); ++i) { 
    //        for (int j = 0; j < min(heights[i], (int)spinResults.size()); ++j) {
    //            grid[i][j] = spinResults[i][j];
    //        }
    //    }
    //}

    // Method to generate the screen based on the chosen indices for spinning the reels
    void generateScreen(ReelSet& reelSet) {
        clearScreen();
        for (int reelIndex = 0; reelIndex < numReels; ++reelIndex) {
            for (int rowIndex = 0; rowIndex < heights[reelIndex]; ++rowIndex) {
                int currentIndex = (reelSet.currentIndices[reelIndex] + rowIndex) % reelSet.reels[reelIndex].symbols.size();
                updateCell(reelIndex, rowIndex, reelSet.reels[reelIndex].symbols[currentIndex]);
            }
        }
    }


    // Function to count the number of times a symbol appears on a reel
    int countSymbolOnReel(int reelIndex, const string& symbol, bool includeWild = true) const {
        if (reelIndex < 0 || reelIndex >= numReels) {
            //  cerr << "Invalid reel index" << endl;
            return 0;
        }
        int count = 0;
        //for (int i = 0; i < heights[reelIndex]; ++i) {
        //    if (includeWild) {
        //        if (grid[reelIndex][i] == symbol || grid[reelIndex][i] == "WL") {
        //            ++count;
        //        }
        //    }
        //    else {
        //        if (grid[reelIndex][i] == symbol) {
        //            ++count;
        //        }
        //    }
        //}
        // 1) vertical column
        for (int row = 0; row < heights[reelIndex]; ++row) {
            if (match(grid[reelIndex][row], symbol, includeWild)) ++count;
        }
        // 2) side rows
        if (middleReel(reelIndex)) {
            if (match(overRow[reelIndex - 1].name, symbol, includeWild)) ++count;
            if (match(underRow[reelIndex - 1].name, symbol, includeWild)) ++count;
        }
        return count;
    }

   

    // Function to count the number of times a symbol appears on the screen
    int countSymbolOnScreen(const string& symbol, bool includeWild = true) const {
        int count = 0;
        for (int i = 0; i < numReels; ++i) {
            count += countSymbolOnReel(i, symbol, includeWild);
        }
        return count;
    }

    // Function to count the length and number of ways for a given symbol
    pair<int, int> getWaysForSymbol(const string& symbol)const {
        int length = 0;
        int ways = 1;
        for (int i = 0; i < numReels; ++i) {
            int count = countSymbolOnReel(i, symbol);
            if (count > 0) {
                length++;
                ways *= count;
            }
            else
                break;
        }
        if (length == 0) {
            ways = 0;
        }
        return make_pair(length, ways);
    }

   
    json toJson(bool includeOver = false, bool includeUnder = false) const {
        json screenJson;

        if (includeOver) {
            json overJson = json::array();
            overJson.push_back("-");
            for (int i = 0; i < SIDE_LEN; ++i) {
                const auto& c = overRow[i];
                overJson.push_back(c.boosted ? (c.name + "*") : c.name);
            }
            overJson.push_back("-");
            screenJson.push_back(overJson);
		}

        // Convert the grid into a JSON array of arrays
        for (int i = 0; i < maxHeight; ++i) {
            json rowJson = json::array();
            for (int j = 0; j < numReels; ++j) {
                if (i >= heights[j]) {
					rowJson.push_back("-"); // Might need to change spacing
					//continue;
				} else
                rowJson.push_back(grid[j][i]);
            }
            screenJson.push_back(rowJson);
        }
        if (includeUnder) {
            json underJson = json::array();
            underJson.push_back("-");
            for (int i = 0; i < SIDE_LEN; ++i) {
                const auto& c = underRow[i];
                underJson.push_back(c.boosted ? (c.name + "*") : c.name);
            }
            underJson.push_back("-");
            screenJson.push_back(underJson);
        }

        return screenJson;
    }

    void cascadeSideRow(bool over, ReelSet& rs, int boostProb)
    {
        auto& row = over ? overRow : underRow;
        const auto& strip = rs.reels[0].symbols;
        const int N = static_cast<int>(strip.size());
        if (N == 0) return;

        // left = index for row[0]; next = symbol immediately AFTER the rightmost
        int left = rs.currentIndices[0];
        int next = (left + SIDE_LEN) % N;      // <-- start AFTER the visible window

        for (int pos = 0; pos < SIDE_LEN; ++pos) {
            while (row[pos].name.empty()) {
                // shift visible window one step LEFT
                for (int p = pos; p < SIDE_LEN - 1; ++p)
                    row[p] = row[p + 1];

                // bring the next symbol in on the RIGHT
                //row[SIDE_LEN - 1] = strip[next];
				bool boosted = getRand("TB", 100) < boostProb;
                row[SIDE_LEN - 1] = SideCell{ strip[next], boosted }; 

                // the window advanced by one:
                left = (left + 1) % N;
                next = (next + 1) % N;
            }
        }
        rs.currentIndices[0] = left;           // <-- persist new leftmost index
    }



    void cascadeSymbols(ReelSet& reelSet, bool useDifferentReelSet, ReelSet& alternateReelSet) {
        ReelSet& activeReelSet = useDifferentReelSet ? alternateReelSet : reelSet;

        for (int reel = 0; reel < numReels; ++reel) {
            for (int row = heights[reel] - 1; row >= 0; --row) {
                while (grid[reel][row] == "") {
                    // Shift symbols above down to fill this empty position
                    for (int aboveRow = row; aboveRow > 0; aboveRow--) {
                        grid[reel][aboveRow] = grid[reel][aboveRow - 1];
                    }
                    activeReelSet.currentIndices[reel]--;
                    if (activeReelSet.currentIndices[reel] < 0) {
                        activeReelSet.currentIndices[reel] = activeReelSet.reels[reel].symbols.size() - 1;
                    }
                    // Fill the topmost position with a new symbol
                    grid[reel][0] = activeReelSet.reels[reel].symbols[activeReelSet.currentIndices[reel]];
                    
                }
            }
        }
    }
   

    // New method to add side symbols from an integrated ReelSet
    void addSideSymbolsFromIntegratedReelSet(const ReelSet& rs,
        const std::vector<bool>& overBoostVec = { 0,0,0,0 },
        const std::vector<bool>& underBoostVec = { 0,0,0,0 }) {
        // Add over symbols if the reelset has them
        if (rs.hasOverReel()) {
            const auto& overStrip = rs.getOverReel()->symbols;
            for (int i = 0; i < SIDE_LEN; ++i) {
                setSideSymbol(true, i,
                    overStrip[(rs.currentOverIndex + i) % overStrip.size()],
                    overBoostVec[i]);
            }
        }

        // Add under symbols if the reelset has them
        if (rs.hasUnderReel()) {
            const auto& underStrip = rs.getUnderReel()->symbols;
            for (int i = 0; i < SIDE_LEN; ++i) {
                setSideSymbol(false, i,
                    underStrip[(rs.currentUnderIndex + i) % underStrip.size()],
                    underBoostVec[i]);
            }
        }
    }

    // Modified cascade method for integrated over/under reels
    void cascadeSideRowIntegrated(bool over, ReelSet& rs, int boostProb) {
        // Check if this reelset has the requested side reel
        if (over && !rs.hasOverReel()) return;
        if (!over && !rs.hasUnderReel()) return;

        auto& row = over ? overRow : underRow;
        const auto& strip = over ? rs.getOverReel()->symbols : rs.getUnderReel()->symbols;
        const int N = static_cast<int>(strip.size());
        if (N == 0) return;

        // Get current index
        int& currentIndex = over ? rs.currentOverIndex : rs.currentUnderIndex;

        // left = index for row[0]; next = symbol immediately AFTER the rightmost
        int left = currentIndex;
        int next = (left + SIDE_LEN) % N;      // <-- start AFTER the visible window

        for (int pos = 0; pos < SIDE_LEN; ++pos) {
            while (row[pos].name.empty()) {
                // shift visible window one step LEFT
                for (int p = pos; p < SIDE_LEN - 1; ++p)
                    row[p] = row[p + 1];

                // bring the next symbol in on the RIGHT
                //bool boosted = getRand("TB_" + (over) ? "O" : "U", 100) < boostProb;
                bool boosted = (boostProb == 100) ||
                               ( getRand(std::string("BoostT_") + (over ? "O" : "U"), 100) < boostProb );
                row[SIDE_LEN - 1] = SideCell{ strip[next], boosted };

                // the window advanced by one:
                left = (left + 1) % N;
                next = (next + 1) % N;
            }
        }
        currentIndex = left;           // <-- persist new leftmost index
    }

    // Alternative: Keep your existing addSideSymbols method for backward compatibility
    // and add an overload for integrated reelsets:
    void addSideSymbols(bool over, const ReelSet& rs, const std::vector<bool>& boostVec = { 0,0,0,0 }) {
        // Check if this is an integrated reelset with over/under reels
        if (over && rs.hasOverReel()) {
            const auto& strip = rs.getOverReel()->symbols;
            for (int i = 0; i < SIDE_LEN; ++i)
                setSideSymbol(over, i, strip[(rs.currentOverIndex + i) % strip.size()], boostVec[i]);
        }
        else if (!over && rs.hasUnderReel()) {
            const auto& strip = rs.getUnderReel()->symbols;
            for (int i = 0; i < SIDE_LEN; ++i)
                setSideSymbol(over, i, strip[(rs.currentUnderIndex + i) % strip.size()], boostVec[i]);
        }
        else {
            // Fallback to old behavior for backward compatibility
            // (assuming single reel in reels[0] contains the side symbols)
            const auto& strip = rs.reels[0].symbols;
            for (int i = 0; i < SIDE_LEN; ++i)
                setSideSymbol(over, i, strip[(rs.currentIndices[0] + i) % strip.size()], boostVec[i]);
        }
    }

    void markPosition(int reel, int row) {
		markedPositions.push_back(make_pair(reel, row));
	}

    void clearMarkedPositions() {
        markedPositions.clear();
    }
    
    // Get marked positions
    const std::vector<std::pair<int, int>>& getMarkedPositions() const {
		return markedPositions;
	}

    // Mark given symbol up to length on the screen. includeWild as parameter
    void markSymbol(const string& symbol, int length, bool includeWild = true) {        
        for (int i = 0; i < length; ++i) {
            for (int j = 0; j < heights[i]; ++j) {
                if (grid[i][j] == symbol || (includeWild && grid[i][j] == "WL")) {
                    markedPositions.push_back(make_pair(i, j));
                }
            }
            if (middleReel(i)) {
                if (getSideSymbol(true, i - 1) == symbol || (includeWild && getSideSymbol(true, i - 1) == "WL"))
                    markedPositions.emplace_back(i, -1);            // -1  = overRow
                if (getSideSymbol(false, i - 1) == symbol || (includeWild && getSideSymbol(false, i - 1) == "WL"))
                    markedPositions.emplace_back(i, -2);    // underRow sentinel
            }
        }
    }

    void removeMarkedPositions() {
        for (const auto& position : markedPositions) {
            int reel = position.first;
            int row = position.second;
            if (row >= 0 && row < heights[reel]) {
                grid[reel][row] = "";  // Clear the winning symbol in the grid
            } else if (middleReel(reel)) {
                if (row == -1) {
                    overRow[reel - 1].name = ""; overRow[reel - 1].boosted = false; }
                else if (row == -2) { 
                    underRow[reel - 1].name = ""; underRow[reel - 1].boosted = false; }
            }
        }
    }

    // Function to fill all marked symbols with a specified symbol
    void fillMarkedSymbols(const string& symbol) {
        for (const auto& position : markedPositions) {
			int reel = position.first;
			int row = position.second;
			grid[reel][row] = symbol;
		}
	}
};

