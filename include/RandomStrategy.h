#ifndef RANDOMSTRATEGY50_H
#define RANDOMSTRATEGY50_H

#include "Strategy.h"
#include "Position.h"
#include "Config.h"
#include "Utils.h"
#include <random>

class RandomStrategy50 : public Strategy {
private:
    // --- Parameters ---
    double entryProbability = 0.01;    // Chance per bar to attempt entry
    double benchmarkFixedSize = 1.0;   // Fixed position size
    bool debugMode = false;            // Debug output flag
    
    // --- State ---
    Position currentPosition;
    bool inPosition = false;
    int currentPendingOrderId = -1;
    double startingAccountValue = 0.0;
    int tradeCount = 0;
    int profitableTrades = 0;
    
    // --- Random Number Generation ---
    std::mt19937 rng;
    std::uniform_real_distribution<double> probDist;   // For entry probability
    std::uniform_int_distribution<int> directionDist;  // For long/short decision
    std::uniform_int_distribution<int> outcomeGen;     // For 50/50 win/loss outcome

public:
    RandomStrategy50();
    std::string getName() { return "Random50"; }
    
    // --- Overridden Lifecycle Methods ---
    void init() override;
    void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // RANDOMSTRATEGY50_H
