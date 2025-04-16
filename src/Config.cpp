// Config.cpp
#include "Config.h"
#include "Utils.h" 
#include "json.hpp"
#include <fstream>
#include <iomanip>     // For std::setw in saving JSON
#include <stdexcept>
#include <cstdint> 

using json = nlohmann::json;

// --- Constructor ---
Config::Config() {
    // configFilePath = "config.json";
    setDefaultValues(); // Start with default structure
}

// --- Set Defaults ---
void Config::setDefaultValues() {
    configData = {
        {"Data", {
            {"SourceType", "CSV"}, // Added: Type of source (CSV, WebSocket, Database etc.) - For future use
            {"INPUT_CSV_PATH", "E:/UMHack/btc_market_data.csv"},
            {"USE_PARTIAL_DATA", false},
            {"PARTIAL_DATA_PERCENT", 100.0},
            {"USE_PARTIAL_DATA", false},
            {"PARTIAL_DATA_PERCENT", 100.0},

            {"CSV_Timestamp_Col", 0},        // Column index (0-based) or Name (if header exists)
            {"CSV_Timestamp_Format", "%Y-%m-%d %H:%M:%S"}, // strptime/get_time format + "%f" for custom ms handling
            // {"CSV_Open_Col", 1},              // Optional: Use -1 if not present or calculated
            {"CSV_Close_Col", 1},
            // {"CSV_Volume_Col", 3},
            // {"CSV_High_Col", -1},
            // {"CSV_Close_Col", -1},
            {"CSV_Delimiter", ","},
            {"CSV_Has_Header", true}
        }},
        {"Broker", {
            {"STARTING_CASH", 100000.0},
            {"STARTING_CASH", 100000.0},
            {"LEVERAGE", 100.0},
            {"COMMISSION_RATE", 0.06}
        }},
        {"Strategy", {
            {"STRATEGY_NAME", "Random"},
            {"DEBUG_MODE", false},
            // {"SMA_PERIOD", 36000},
            {"POSITION_TYPE", "fixed"}, // "fixed", "percent", "risk"
            {"FIXED_SIZE", 20.0},
            // {"CASH_PERCENT", 2.0},
            // {"RISK_PERCENT", 1.0},
            {"STOP_LOSS_PIPS", 50.0},
            {"STOP_LOSS_ENABLED", true},
            {"TAKE_PROFIT_ENABLED", true},
            {"TAKE_PROFIT_PIPS", 50.0},
            
            {"BANKRUPTCY_PROTECTION", true},
            {"FORCE_EXIT_PERCENT", -50.0},
            {"ONE_TRADE", true},
        }}
    };
}


// --- Load from File ---
bool Config::loadFromFile(const std::string& filename) {
    configFilePath = filename;
    std::ifstream ifs(filename);

    if (!ifs.is_open()) {
        Utils::logMessage("Config Warning: Config file not found at '" + filename + "'.");
        Utils::logMessage("Config: Creating default config file...");
        setDefaultValues();
        if (saveToFile(filename)) {
            Utils::logMessage("Config: Default config file created successfully at '" + filename + "'.");
            return true; 
        } else {
            Utils::logMessage("Config Error: Failed to create default config file at '" + filename + "'. Using internal defaults.");
            return false; // Indicate failure to load/create persisted config
        }
    }

    try {
        Utils::logMessage("Config: Loading configuration from '" + filename + "'...");
        json loadedData = json::parse(ifs);
        ifs.close();

        // Merge loaded data with defaults: ensures all keys exist, loaded values override
        // This uses the json::update method (might need specific version or patch)
        // Simpler approach: just replace if loaded successfully
        // configData.update(loadedData); // More robust merging

        // Simpler: Overwrite defaults completely with loaded data
        configData = loadedData;

        // Optional: Validate loaded structure against default keys? More complex.
        Utils::logMessage("Config: Configuration loaded successfully.");
        return true;

    } catch (json::parse_error& e) {
        Utils::logMessage("Config Error: Failed to parse JSON config file '" + filename + "'.");
        Utils::logMessage("  Parse error: " + std::string(e.what()));
        Utils::logMessage("  Using internal default values.");
        ifs.close();
        setDefaultValues();
        return false;
    } catch (const std::exception& e) {
        Utils::logMessage("Config Error: An unexpected error occurred loading config '" + filename + "': " + e.what());
        Utils::logMessage("  Using internal default values.");
        if(ifs.is_open()) ifs.close();
        setDefaultValues();
        return false;
    }
}

// --- Save to File ---
bool Config::saveToFile(const std::string& filename) const {
    std::string savePath = filename.empty() ? configFilePath : filename;
    if (savePath.empty()) {
        Utils::logMessage("Config Error: Cannot save config, no file path specified.");
        return false;
    }

    std::ofstream ofs(savePath);
    if (!ofs.is_open()) {
        Utils::logMessage("Config Error: Could not open file for writing: '" + savePath + "'.");
        return false;
    }

    try {
        // Use std::setw(4) for pretty printing with 4-space indent
        ofs << std::setw(4) << configData << std::endl;
        ofs.close();
        return true;
    } catch (const std::exception& e) {
        Utils::logMessage("Config Error: Failed to write JSON to file '" + savePath + "': " + e.what());
        if(ofs.is_open()) ofs.close();
        return false;
    }
}

// --- Getters ---
template <typename T>
T Config::get(const std::string& key, const T& defaultValue) const {
    // This simple getter only works for top-level keys
    if (configData.contains(key)) {
        try {
            return configData.at(key).get<T>();
        } catch (const json::exception& e) {
            Utils::logMessage("Config Warning: Type mismatch or error getting key '" + key + "'. Using default. Error: " + e.what());
            return defaultValue;
        }
    }
    Utils::logMessage("Config Warning: Key '" + key + "' not found. Using default value.");
    return defaultValue;
}

template <typename T>
T Config::getNested(const std::string& keyPath, const T& defaultValue) const {
    try {
        // Use JSON pointer syntax like "/Data/INPUT_CSV_PATH"
        json::json_pointer ptr(keyPath); // Construct pointer object
        if (configData.contains(ptr)) { // Check using pointer
            return configData.at(ptr).get<T>(); // Access using pointer
        } else {
            //Utils::logMessage("Config Warning: Nested key path '" + keyPath + "' not found. Using default value.");
            return defaultValue;
        }
    } catch (const json::parse_error& pe) { // Catch pointer format errors
        Utils::logMessage("Config Warning: Invalid key path format '" + keyPath + "'. Using default. Error: " + pe.what());
        return defaultValue;
    } catch (const json::exception& e) { // Catch other json errors (type, out_of_range)
        Utils::logMessage("Config Warning: Error accessing nested key '" + keyPath + "'. Using default. Error: " + e.what());
        return defaultValue;
    }
}

// Convenience string getter
std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
    // Decide if you want simple top-level or nested by default for getString
    // Using nested: Assumes keys are like "/Section/Key"
    // Construct the full path if needed, e.g., "/TopLevel/" + key
    // For now, let's assume getString refers to top-level for backward compatibility maybe?
    // Or require full path like "/Strategy/POSITION_TYPE"? Let's require full path.
    // return getNested<std::string>(key, defaultValue); // Use this if key is expected like "/Strategy/POS..."

    // Using simple top-level: assumes key is just "STRATEGY_NAME"
    return get<std::string>(key, defaultValue);
}


// --- Setter ---
template <typename T>
void Config::set(const std::string& key, const T& value) {
    // Similar decision for set: top-level or nested path?
    // Assuming top-level for now. Use json_pointer for nested.
    try {
        configData[key] = value;
    } catch (const json::exception& e) {
        Utils::logMessage("Config Error: Failed to set key '" + key + "'. Error: " + e.what());
    }
    // Example nested set:
    // try {
    //     json::json_pointer ptr(key); // key = "/Strategy/DEBUG_MODE"
    //     configData[ptr] = value;
    // } catch(...) { ... }
}


// --- Has ---
bool Config::has(const std::string& key) const {
    // Similar decision: top-level or nested? Assume top-level matching get/set.
    return configData.contains(key);
    // Example nested has:
    // try {
    //     json::json_pointer ptr(key); // key = "/Strategy/DEBUG_MODE"
    //     return configData.contains(ptr);
    // } catch (...) { return false; }
}


// --- Explicit Template Instantiations ---
template std::string Config::get<std::string>(const std::string& key, const std::string& defaultValue) const;
template double Config::get<double>(const std::string& key, const double& defaultValue) const;
template int Config::get<int>(const std::string& key, const int& defaultValue) const;
template bool Config::get<bool>(const std::string& key, const bool& defaultValue) const;

template std::string Config::getNested<std::string>(const std::string& keyPath, const std::string& defaultValue) const;
template double Config::getNested<double>(const std::string& keyPath, const double& defaultValue) const;
template int Config::getNested<int>(const std::string& keyPath, const int& defaultValue) const;
template bool Config::getNested<bool>(const std::string& keyPath, const bool& defaultValue) const;

template void Config::set<std::string>(const std::string& key, const std::string& value);
template void Config::set<double>(const std::string& key, const double& value);
template void Config::set<int>(const std::string& key, const int& value);
template void Config::set<bool>(const std::string& key, const bool& value);