// BacktestEngine.cpp
#include "BacktestEngine.h"
#include "Utils.h"
#include "Config.h"
#include <stdexcept>
#include <iostream>
#include <chrono>

// --- Constructor ---
// Initialize members, especially DataLoader and Broker
BacktestEngine::BacktestEngine(const Config& cfg) : // Take const ref
    config(cfg), // Copy config
    currentBarIndex(0),
    dataLoader(cfg)
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

    // Extract primary data name
    std::string path = config.getNested<std::string>("/Data/INPUT_CSV_PATH", "");
    size_t last_slash_idx = path.find_last_of("\\/");
    std::string filename = (std::string::npos == last_slash_idx) ? path : path.substr(last_slash_idx + 1);
    // Simple parsing assuming "SYMBOL_..." format 
    size_t first_underscore = filename.find('_');
    primaryDataName = (std::string::npos == first_underscore) ? filename : filename.substr(0, first_underscore);

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
    strategy = std::move(strat);
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
    size_t totalBars = historicalData.size();
    Utils::logMessage("Beginning backtest with " + std::to_string(totalBars) + " total bars");
    
    for (currentBarIndex = 0; currentBarIndex < totalBars; ++currentBarIndex) {
        const Bar& currentBar = historicalData[currentBarIndex];

        if (currentBarIndex % 500 == 0) {
            Utils::logMessage("Processing bar " + std::to_string(currentBarIndex) + "/" + 
                              std::to_string(totalBars) + " - Date: " + Utils::timePointToString(currentBar.timestamp));
        }

        // 1. Update current prices map (simple version: only primary data)
        try {
            // Utils::logMessage("Updating prices...");
            if (currentBar.columns.size() <= 1) {
                Utils::logMessage("BacktestEngine Error: Insufficient columns (" + std::to_string(currentBar.columns.size()) + ") for price update at bar " + std::to_string(currentBarIndex));
                continue;
            }
            currentPrice= currentBar.columns[1];
            // Utils::logMessage("Updated price for " + primaryDataName + ": " + std::to_string(currentPrice));
        } catch (const std::exception& e) {
            Utils::logMessage("BacktestEngine Error: Exception updating prices: " + std::string(e.what()));
            continue; // Skip to next bar if we can't update prices
        }

        // 2. Process broker orders based on current bar's data
        bool brokerError = false;
        try {
            Utils::logMessage("Processing broker orders...");
            broker->processOrders(currentBar);
        } catch (const std::exception& e) {
            brokerError = true;
            Utils::logMessage("BacktestEngine Error: Exception during broker processing: " + std::string(e.what()));
            std::cerr << "Exception in broker processing: " << e.what() << std::endl;
        } catch (...) {
            brokerError = true;
            Utils::logMessage("BacktestEngine Error: Unknown exception during broker processing");
            std::cerr << "Unknown exception in broker processing" << std::endl;
        }

        // Skip strategy execution if broker processing had errors
        if (brokerError) {
            continue;
        }

        // 3. Call strategy's next logic
        try {
            // Reduce verbose logging for better performance
            // if (currentBarIndex % 500 == 0) {
            //     Utils::logMessage("Calling strategy->next for bar " + std::to_string(currentBarIndex));
            // }
            strategy->next(currentBar, currentBarIndex, currentPrice);
            // if (currentBarIndex % 500 == 0) {
            //     Utils::logMessage("Completed strategy->next call successfully");
            // }
        } catch (const std::exception& e) {
            Utils::logMessage("BacktestEngine Error: Exception during strategy next(): " + std::string(e.what()));
            std::cerr << "Exception in strategy next(): " << e.what() << std::endl;
        } catch (...) {
            Utils::logMessage("BacktestEngine Error: Unknown exception during strategy next()");
            std::cerr << "Unknown exception in strategy next()" << std::endl;
        }

        // Add progress reporting - important for debugging but reduce frequency
        // if (currentBarIndex % 500 == 0 || currentBarIndex == totalBars - 1) {
        //     Utils::logMessage("Progress: Bar " + std::to_string(currentBarIndex + 1) + "/" + 
        //                       std::to_string(totalBars) + " processed");
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