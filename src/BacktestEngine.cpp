// BacktestEngine.cpp
#include "BacktestEngine.h"
#include "Utils.h"
#include "SmaCrossStrategy.h" // Include concrete strategy for now
#include <stdexcept>
#include <iostream> // For error messages
#include <chrono>   // For timing the backtest

// --- Constructor ---
// Initialize members, especially DataLoader and Broker
BacktestEngine::BacktestEngine(const Config& cfg) : // Take const ref
    config(cfg), // Copy config
    // Use getNested with JSON pointer syntax "/"
    dataLoader(config),
    currentBarIndex(0)
{
    // Initialize Broker using config values
    try {
        double startCash = config.getNested<double>("/Broker/STARTING_CASH", 1000.0);
        double leverage = config.getNested<double>("/Broker/LEVERAGE", 100.0);
        double commRate = config.getNested<double>("/Broker/COMMISSION_RATE", 0.0);
        broker = std::make_unique<Broker>(startCash, leverage, commRate);
    } catch (const std::exception& e) {
         // Catch potential type errors from getNested as well
        Utils::logMessage("BacktestEngine Error: Failed to parse broker parameters from config: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize Broker from config.");
    }

    // Extract primary data name (can still do this from path)
    std::string path = config.getNested<std::string>("/Data/INPUT_CSV_PATH", "");
    size_t last_slash_idx = path.find_last_of("\\/");
    std::string filename = (std::string::npos == last_slash_idx) ? path : path.substr(last_slash_idx + 1);
    // Simple parsing assuming "SYMBOL_..." format like python script
    size_t first_underscore = filename.find('_');
    primaryDataName = (std::string::npos == first_underscore) ? "UNKNOWN_DATA" : filename.substr(0, first_underscore);


    Utils::logMessage("BacktestEngine initialized for data: " + primaryDataName);
}

// --- Setup ---
bool BacktestEngine::loadData() {
    Utils::logMessage("BacktestEngine: Loading data...");
    try {
        // Use getNested here too
        bool usePartial = config.getNested<bool>("/Data/USE_PARTIAL_DATA", false);
        double partialPercent = 100.0;
        if (usePartial) {
            partialPercent = config.getNested<double>("/Data/PARTIAL_DATA_PERCENT", 100.0);
        }
        // dataLoader was already initialized with the path from config
        historicalData = dataLoader.loadData(usePartial, partialPercent);

        if (historicalData.empty()) {
             Utils::logMessage("BacktestEngine Error: No data loaded from file.");
             return false;
        }
         Utils::logMessage("BacktestEngine: Data loaded successfully (" + std::to_string(historicalData.size()) + " bars).");
        return true;

    } catch (const std::exception& e) {
        Utils::logMessage("BacktestEngine Error: Failed to load data: " + std::string(e.what()));
        return false;
    }
    return !historicalData.empty(); // Simplified return
}

void BacktestEngine::setStrategy(std::unique_ptr<Strategy> strat) {
    if (!strat) {
        Utils::logMessage("BacktestEngine Warning: Attempted to set a null strategy.");
        return;
    }
     Utils::logMessage("BacktestEngine: Setting strategy...");
    strategy = std::move(strat); // Take ownership
}

// --- Execution ---
void BacktestEngine::run() {
    Utils::logMessage("--- Starting Backtest Run ---");
    auto startTime = std::chrono::high_resolution_clock::now();

    // --- Pre-run Checks ---
    if (historicalData.empty()) {
        Utils::logMessage("BacktestEngine Error: Cannot run without historical data.");
        return;
    }
    if (!strategy) {
        Utils::logMessage("BacktestEngine Error: Cannot run without a strategy set.");
        return;
    }
     if (!broker) {
          Utils::logMessage("BacktestEngine Error: Broker not initialized.");
          return;
     }

    // --- Setup Links ---
    Utils::logMessage("BacktestEngine: Linking components...");
    strategy->setBroker(broker.get()); // Pass raw pointer
    strategy->setData(&historicalData, primaryDataName); // Pass pointer to data and name
    strategy->setConfig(&config); // Pass pointer to config
    broker->setStrategy(strategy.get()); // Pass raw pointer

    // --- Initialize Strategy ---
     Utils::logMessage("BacktestEngine: Initializing strategy...");
     try {
         strategy->init();
     } catch (const std::exception& e) {
         Utils::logMessage("BacktestEngine Error: Exception during strategy init: " + std::string(e.what()));
         return; // Stop run if init fails
     }


    // --- Main Backtest Loop ---
     Utils::logMessage("BacktestEngine: Starting event loop...");
     size_t totalBars = historicalData.size();
     for (currentBarIndex = 0; currentBarIndex < totalBars; ++currentBarIndex) {
        const Bar& currentBar = historicalData[currentBarIndex];

        // 1. Update current prices map (simple version: only primary data)
        currentPrices[primaryDataName] = currentBar.close;

        // 2. Process broker orders based on current bar's data
        //    (Broker uses bar data for fills, e.g., currentBar.close)
        try {
            broker->processOrders(currentBar);
        } catch (const std::exception& e) {
             Utils::logMessage("BacktestEngine Error: Exception during broker processing: " + std::string(e.what()));
             // Decide whether to stop or continue
             break; // Stop for safety
        }


        // 3. Call strategy's next logic
        try {
             // Pass current prices map in case strategy needs other data prices (not used by SMA cross)
            strategy->next(currentBar, currentPrices);
        } catch (const std::exception& e) {
             Utils::logMessage("BacktestEngine Error: Exception during strategy next(): " + std::string(e.what()));
              // Decide whether to stop or continue
             break; // Stop for safety
        }


        // Optional: Progress reporting
        // if (currentBarIndex % 10000 == 0) { // Report every N bars
        //     Utils::logMessage("Progress: Bar " + std::to_string(currentBarIndex) + "/" + std::to_string(totalBars));
        // }

     } // End of main loop

    // --- Post-Loop ---
    Utils::logMessage("BacktestEngine: Event loop finished.");

    // --- Final Strategy Call ---
     Utils::logMessage("BacktestEngine: Calling strategy stop()...");
     try {
         strategy->stop();
     } catch (const std::exception& e) {
          Utils::logMessage("BacktestEngine Error: Exception during strategy stop(): " + std::string(e.what()));
     }


    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;
    Utils::logMessage("--- Backtest Run Finished ---");
    Utils::logMessage("Total Execution Time: " + std::to_string(duration.count()) + " seconds");
}