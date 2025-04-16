// main.cpp
#include "Config.h"
#include "BacktestEngine.h"
#include "RandomStrategy.h"
#include "Utils.h"
#include <iostream>
#include <memory>
#include <string>
#include <limits>
#include "ModelInterface.cpp"
#include "../include/OnnxModelInterface.h"

#ifndef PYTHON_BINDINGS
// Helper function to wait for user input before exiting
void waitForKeypress() {
    Utils::logMessage("Program finished - waiting for user input before closing");
    std::cout << "\nPress Enter key to close..." << std::endl;

    std::cin.clear();
    // std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    getchar();
}

int main() {
    try {
        Utils::logMessage("--- C++ Backtester Starting ---");

        // 1. Create and Load Configuration
        Config config;
        std::string configFilename = "config.json"; // Define config file name
        if (!config.loadFromFile(configFilename)) {
            // Log message already handled in loadFromFile if creation failed
            Utils::logMessage("Main Warning: Proceeding with internal default configuration.");
            std::cout << "Warning: Using default configuration as config.json couldn't be loaded/created." << std::endl;
        } else {
            std::cout << "Configuration loaded successfully from " << configFilename << std::endl;
        }

        // Example: Override a value after loading (e.g., from command line later)
        // config.set<bool>("/Strategy/DEBUG_MODE"_json_pointer, true);

        // 2. Create Backtest Engine (Pass the loaded config)
        std::unique_ptr<BacktestEngine> engine;
        try {
            std::cout << "Creating backtest engine..." << std::endl;
            engine = std::make_unique<BacktestEngine>(config); // Pass config object by const reference
        } catch (const std::exception& e) {
            Utils::logMessage("Main Error: Failed to create Backtest Engine: " + std::string(e.what()));
            std::cerr << "Main Error: Failed to create Backtest Engine: " << e.what() << std::endl;
            waitForKeypress();
            return 1;
        }

        // 3. Load Data into Engine (Engine gets path from its config copy)
        std::cout << "Loading market data..." << std::endl;
        if (!engine->loadData()) {
            Utils::logMessage("Main Error: Failed to load data. Exiting.");
            std::cerr << "Main Error: Failed to load data. Exiting." << std::endl;
            waitForKeypress();
            return 1;
        }

        // 3.5 See if ONNX runtime is setup correctly
        // OnnxModelInterface model = OnnxModelInterface();
        // model.PrintModelInfo();

        // 4. Create and Set Strategy
        //    (Engine passes its config pointer to the strategy during setup)
        auto strategy = std::make_unique<RandomStrategy>();
        Utils::logMessage("Main: Creating " + strategy->getName() + " strategy.");
        std::cout << "Creating " << strategy->getName() << " strategy..." << std::endl;
        engine->setStrategy(std::move(strategy));

        // 5. Run the Backtest
        std::cout << "Starting backtest..." << std::endl;
        engine->run();
        std::cout << "Backtest completed successfully!" << std::endl;

        Utils::logMessage("--- C++ Backtester Finished ---");
    }
    catch (const std::exception& e) {
        Utils::logMessage("Main Error: Uncaught exception: " + std::string(e.what()));
        std::cerr << "Main Error: Uncaught exception: " << e.what() << std::endl;
        waitForKeypress();
        return 1;
    }
    catch (...) {
        Utils::logMessage("Main Error: Unknown exception occurred");
        std::cerr << "Main Error: Unknown exception occurred" << std::endl;
        waitForKeypress();
        return 1;
    }

    std::cout << "About to call waitForKeypress()..." << std::endl;
    waitForKeypress();
    return 0;
}
#endif