// main.cpp
#include "Config.h"
#include "BacktestEngine.h"
#include "SmaCrossStrategy.h"
#include "RandomStrategy50.h"
#include "Utils.h"
#include <iostream>
#include <memory>
#include <string> // Include string

// Helper function to wait for user input before exiting
void waitForKeypress() {
    std::cout << "\nPress any key to close..." << std::endl;
    std::cin.get();
}

int main() {
    Utils::logMessage("--- C++ Backtester Starting ---");

    // 1. Create and Load Configuration
    Config config;
    std::string configFilename = "config.json"; // Define config file name
    if (!config.loadFromFile(configFilename)) {
        // Log message already handled in loadFromFile if creation failed
        // Decide if you want to exit if loading/creation failed.
        // For now, it continues with internal defaults if file couldn't be saved.
        Utils::logMessage("Main Warning: Proceeding with internal default configuration.");
    }

    // Example: Override a value after loading (e.g., from command line later)
    // config.set<bool>("/Strategy/DEBUG_MODE"_json_pointer, true); // Using json_pointer syntax

    // 2. Create Backtest Engine (Pass the loaded config)
    std::unique_ptr<BacktestEngine> engine;
    try {
        engine = std::make_unique<BacktestEngine>(config); // Pass config object by const reference
    } catch (const std::exception& e) {
        Utils::logMessage("Main Error: Failed to create Backtest Engine: " + std::string(e.what()));
        std::cerr << "Main Error: Failed to create Backtest Engine: " << e.what() << std::endl;
        waitForKeypress();
        return 1;
    }

    // 3. Load Data into Engine (Engine gets path from its config copy)
    if (!engine->loadData()) {
        Utils::logMessage("Main Error: Failed to load data. Exiting.");
        std::cerr << "Main Error: Failed to load data. Exiting." << std::endl;
        waitForKeypress();
        return 1;
    }

    // 4. Create and Set Strategy
    //    (Engine passes its config pointer to the strategy during setup)
    auto strategy = std::make_unique<RandomStrategy50>();
    Utils::logMessage("Main: Creating " + strategy->getName() + " strategy.");
    engine->setStrategy(std::move(strategy));

    // 5. Run the Backtest
    try {
        engine->run();
    } catch (const std::exception& e) {
        Utils::logMessage("Main Error: Uncaught exception during engine run: " + std::string(e.what()));
        std::cerr << "Main Error: Uncaught exception during engine run: " << e.what() << std::endl;
        waitForKeypress();
        return 1;
    }

    Utils::logMessage("--- C++ Backtester Finished ---");
    waitForKeypress();
    return 0;
}