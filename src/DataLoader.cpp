// DataLoader.cpp
#include "DataLoader.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <vector>
#include <charconv>

// Constructor takes Config reference
DataLoader::DataLoader(const Config& cfg) :
    config(cfg),
    filePath(config.getNested<std::string>("/Data/INPUT_CSV_PATH", ""))
{
    if (filePath.empty()) {
        Utils::logMessage("DataLoader Warning: No INPUT_CSV_PATH found in config.");
        // THROW ERROR
    }
}

// Helper to count lines (implementation remains the same)
long long DataLoader::countLines() const {
     if (filePath.empty()) return -1;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return -1;
    }
    long long lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineCount;
    }
    return lineCount;
}

// Internal parsing helper based on config
bool DataLoader::parseLine(const std::string& line, Bar& bar) const {
    char delimiter = config.getNested<std::string>("/Data/CSV_Delimiter", ",")[0];
    int tsCol = config.getNested<int>("/Data/CSV_Timestamp_Col", 0);
    std::string tsFmt = config.getNested<std::string>("/Data/CSV_Timestamp_Format", "%Y%m%d %H%M%S");
    // Utils::logMessage("ts format: " + tsFmt);

    int bidCol = config.getNested<int>("/Data/CSV_Bid_Col", -1);
    int askCol = config.getNested<int>("/Data/CSV_Ask_Col", -1);
    int volCol = config.getNested<int>("/Data/CSV_Volume_Col", -1);

    std::stringstream ss(line);
    std::string field;
    std::vector<std::string> fields;

    while (std::getline(ss, field, delimiter)) {
        // Trim whitespace
        field.erase(0, field.find_first_not_of(" \t\n\r\f\v"));
        field.erase(field.find_last_not_of(" \t\n\r\f\v") + 1);
        fields.push_back(field);
    }

    try {
        // --- Parse Timestamp ---
        // Use the custom Utils::parseTimestamp which handles "%f" manually
        // Note: If Utils::parseTimestamp needs the format string, pass tsFmt to it.
        // Assuming Utils::parseTimestamp is hardcoded to "%Y%m%d %H%M%S%f" for now.
        // TODO: Modify Utils::parseTimestamp to accept a format string if needed.
        if (tsCol < 0 || tsCol >= fields.size()) throw std::out_of_range("Timestamp column index invalid");
        bar.timestamp = Utils::parseTimestamp(fields[tsCol], tsFmt);

        // --- Parse Bid/Ask/Volume ---
        // Using std::from_chars for potentially better performance than stod/stoll
        auto parse_double = [&](int colIndex, double& out_val) {
            if (colIndex < 0 || colIndex >= fields.size()) return false; // Return false if column not configured or invalid
            const std::string& s = fields[colIndex];
            auto result = std::from_chars(s.data(), s.data() + s.size(), out_val);
            // Check if parsing succeeded and consumed the whole string part intended
            if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
                // Let's throw for now if from_chars fails completely.
                if (result.ec == std::errc::result_out_of_range) throw std::out_of_range("Double value out of range");
                throw std::invalid_argument("Invalid double format");
            }
            return true;
        };
        
        auto parse_long = [&](int colIndex, long long& out_val) {
            if (colIndex < 0 || colIndex >= fields.size()) return false; // Return false if column not configured or invalid
            const std::string& s = fields[colIndex];
            auto result = std::from_chars(s.data(), s.data() + s.size(), out_val);
            if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
                if (result.ec == std::errc::result_out_of_range) throw std::out_of_range("Long long value out of range");
                throw std::invalid_argument("Invalid integer format");
            }
            return true;
        };

        // Get OHLC columns
        int openCol = config.getNested<int>("/Data/CSV_Open_Col", -1);
        int highCol = config.getNested<int>("/Data/CSV_High_Col", -1);
        int lowCol = config.getNested<int>("/Data/CSV_Low_Col", -1);
        int closeCol = config.getNested<int>("/Data/CSV_Close_Col", -1);
        
        // Parse OHLC first if available
        bool hasOpen = parse_double(openCol, bar.open);
        bool hasHigh = parse_double(highCol, bar.high);
        bool hasLow = parse_double(lowCol, bar.low);
        bool hasClose = parse_double(closeCol, bar.close);
        
        // Parse bid/ask/volume
        bool hasBid = parse_double(bidCol, bar.bid);
        bool hasAsk = parse_double(askCol, bar.ask);
        bool hasVolume = parse_long(volCol, bar.volume);
        
        // Handle fallbacks based on available data
        if (!hasVolume) {
            bar.volume = 0; // Default volume to 0 if not available
        }
        
        // Apply fallbacks for price data
        if (hasClose) {
            // Use close as fallback for missing bid/ask
            if (!hasBid) bar.bid = bar.close;
            if (!hasAsk) bar.ask = bar.close;
            
            // Use close for missing OHLC components
            if (!hasOpen) bar.open = bar.close;
            if (!hasHigh) bar.high = bar.close;
            if (!hasLow) bar.low = bar.close;
        } 
        else if (hasBid && hasAsk) {
            // No close but have bid/ask - calculate mid for OHLC
            double mid = bar.midPrice();
            if (!hasOpen) bar.open = mid;
            if (!hasHigh) bar.high = mid;
            if (!hasLow) bar.low = mid;
            bar.close = mid;
        }
        else if (hasBid) {
            // Only have bid - use it for everything
            if (!hasAsk) bar.ask = bar.bid;
            if (!hasOpen) bar.open = bar.bid;
            if (!hasHigh) bar.high = bar.bid;
            if (!hasLow) bar.low = bar.bid;
            bar.close = bar.bid;
        }
        else if (hasAsk) {
            // Only have ask - use it for everything
            if (!hasBid) bar.bid = bar.ask;
            if (!hasOpen) bar.open = bar.ask;
            if (!hasHigh) bar.high = bar.ask;
            if (!hasLow) bar.low = bar.ask;
            bar.close = bar.ask;
        }
        else {
            // No price data available
            Utils::logMessage("DataLoader::parseLine Warning: No price data found in line: " + line);
            return false;
        }

        // Final validation
        if (bar.bid == 0.0 && bar.ask == 0.0 && bar.close == 0.0) {
            Utils::logMessage("DataLoader::parseLine Warning: All prices are zero. Line: " + line);
            return false;
        }

        return true;

    } catch (const std::invalid_argument& e) {
        Utils::logMessage("DataLoader::parseLine Warning: Invalid numeric format: " + std::string(e.what()) + ". Line: " + line);
    } catch (const std::out_of_range& e) {
        Utils::logMessage("DataLoader::parseLine Warning: Numeric value out of range or invalid column index: " + std::string(e.what()) + ". Line: " + line);
    } catch (const std::exception& e) { // Catch other errors like timestamp parsing
        Utils::logMessage("DataLoader::parseLine Warning: Error processing line: " + std::string(e.what()) + ". Line: " + line);
    }

    return false;
}


// Load data implementation uses the internal parseLine helper
std::vector<Bar> DataLoader::loadData(bool usePartial, double partialPercent) {

    if (filePath.empty()) {
        Utils::logMessage("DataLoader Error: Cannot load data, file path is not set (check config).");
        return {}; // Return empty vector
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        Utils::logMessage("DataLoader Error: Could not open file: " + filePath); // Use Utils::logMessage
        return {}; // Return empty vector
    }

    Utils::logMessage("DataLoader: Starting data load from " + filePath);

    std::vector<Bar> data;
    std::string line;
    long long linesToRead = -1;
    long long currentLineNum = 0;
    bool skipHeader = config.getNested<bool>("/Data/CSV_Has_Header", false);


    // --- Partial Load Logic (remains similar) ---
    if (usePartial && partialPercent > 0 && partialPercent < 100.0) {
        // ... (counting logic as before) ...
        Utils::logMessage("DataLoader: Partial load requested (" + std::to_string(partialPercent) + "%).");
        long long totalLines = countLines();
        if (totalLines <= 0) {
            Utils::logMessage("DataLoader Warning: Could not count lines or file empty for partial load. Loading full file.");
            linesToRead = -1; // Reset to read all
        } else {
            linesToRead = static_cast<long long>(std::ceil(totalLines * (partialPercent / 100.0)));
            if (linesToRead <= 0) linesToRead = 1;
            Utils::logMessage("DataLoader: Total lines estimated: " + std::to_string(totalLines) + ". Will read approx. " + std::to_string(linesToRead) + " data lines.");
        }
    }

    // --- Main Reading Loop ---
    while (std::getline(file, line)) {
        currentLineNum++;

        // Handle Header
        if (currentLineNum == 1 && skipHeader) {
            Utils::logMessage("DataLoader: Skipping header line.");
            continue; // Skip the first line
        }

        // Handle Partial Load Limit (adjust line count if header was skipped)
        long long effectiveDataLineNum = skipHeader ? currentLineNum - 1 : currentLineNum;
        if (linesToRead > 0 && effectiveDataLineNum > linesToRead) {
             Utils::logMessage("DataLoader: Reached partial load limit of " + std::to_string(linesToRead) + " data lines.");
            break;
        }

        // Basic line validity checks
        if (line.empty() || line[0] == '#') { // Skip empty lines or comments
            continue;
        }

        // Use the parsing helper
        Bar bar;
        if (parseLine(line, bar)) {
            data.push_back(bar);
        } else {
            // parseLine already logs warnings/errors
        }

    }

    Utils::logMessage("DataLoader: Finished loading. Read " + std::to_string(data.size()) + " valid bars from " + std::to_string(currentLineNum) + " total lines processed.");
    return data;
}