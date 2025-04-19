// BacktestEngine.h
#ifndef BACKTESTENGINE_H
#define BACKTESTENGINE_H

#include "Config.h"
#include "Broker.h"
#include "Strategy.h"
#include "Bar.h"
#include "DataLoader.h"
#include <vector>
#include <string>
#include <memory>
#include <map> 

class BacktestEngine {
private:
    Config config; // Store the configuration
    std::vector<Bar> historicalData;
    std::unique_ptr<Broker> broker; // Broker managed by engine
    std::unique_ptr<Strategy> strategy; // Strategy managed by engine
    size_t currentBarIndex;
    std::map<std::string, double> currentPrices; // Map symbol to current price (close)
    double currentPrice;

    DataLoader dataLoader;

    std::string primaryDataName; // Store the name of the main data series

    // Helper to create strategy instance based on config (if needed later)
    // std::unique_ptr<Strategy> createStrategy(const std::string& name);

public:
    // Constructor takes the config object
    explicit BacktestEngine(const Config& cfg);

    // --- Setup ---
    bool loadData(); // Returns true on success
    void setStrategy(std::unique_ptr<Strategy> strat); // Engine takes ownership

    // --- Execution ---
    void run();
};

#endif // BACKTESTENGINE_H