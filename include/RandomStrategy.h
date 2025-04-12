#ifndef RANDOMSTRATEGY_H
#define RANDOMSTRATEGY_H

#include "Strategy.h"
#include "Position.h"
#include "Config.h"
#include "Utils.h"
#include "TradingMetrics.h"
#include <random>
#include <memory>

class RandomStrategy : public Strategy {
private:
    // --- Parameters ---
    double entryProbability = 0.01;    // Chance per bar to attempt entry
    double benchmarkFixedSize = 1.0;   // Fixed position size
    bool debugMode = false;            // Debug output flag
    
    // --- State ---
    Position currentPosition;
    bool inPosition = false;
    int currentOrderId = -1;
    
    // --- Metrics Tracking ---
    std::unique_ptr<TradingMetrics> metrics;
    
    // --- Random Number Generation ---
    std::mt19937 rng;
    std::uniform_real_distribution<double> probDist;   // For entry probability
    std::uniform_int_distribution<int> directionDist;  // For long/short decision

public:
    RandomStrategy();
    virtual std::string getName() const override { return "RandomStrategy"; };
    
    // --- Overridden Lifecycle Methods ---
    void init() override;
    void next(const Bar& currentBar, size_t currentBarIndex, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // RANDOMSTRATEGY_H
