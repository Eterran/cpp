// include/RandomStrategy.h
#ifndef RANDOMSTRATEGY_H
#define RANDOMSTRATEGY_H

#include "Strategy.h"
#include "Position.h"
#include "Config.h"
#include "Utils.h"
#include <random> // For random number generation

class RandomStrategy : public Strategy {
private:
    // --- Parameters ---
    double entryProbability = 0.01; // Chance per bar to attempt entry
    double slTpPips = 100.0;        // SL and TP distance = 100 pips (benchmark)
    double benchmarkFixedSize = 1.0; // Fixed size for benchmark (e.g., 1 unit)
    // General parameters (still useful)
    bool bankruptcyProtection = true;
    double forceExitPercent = -50.0;
    bool debugMode = false;

    // --- State ---
    Position currentPosition;
    bool inPosition = false;
    int currentPendingOrderId = -1;
    double pipPoint = 0.0001;
    double startingAccountValue = 0.0;
    bool bankrupt = false;
    int tradeCount = 0;
    int profitableTrades = 0;

    // --- Random Number Generation ---
    std::mt19937 rng;
    std::uniform_real_distribution<double> probDist;
    std::uniform_int_distribution<int> directionDist;

    // No calculateSize helper needed for fixed size benchmark

public:
    RandomStrategy(); // Constructor

    // --- Overridden Lifecycle Methods ---
    void init() override;
    void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // RANDOMSTRATEGY_H