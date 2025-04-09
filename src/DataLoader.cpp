// DataLoader.cpp
#include "DataLoader.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <vector>
#include <charconv> // For efficient string to number parsing (C++17)

// Constructor takes Config reference
DataLoader::DataLoader(const Config& cfg) :
    config(cfg), // Store reference
    // Get path from config during construction or loading? Let's get it here.
    filePath(config.getNested<std::string>("/Data/INPUT_CSV_PATH", ""))
{
    if (filePath.empty()) {
        Utils::logMessage("DataLoader Warning: No INPUT_CSV_PATH found in config.");
        // Optionally throw, or allow loading to fail later
    }
}


// Helper to count lines (implementation remains the same)
long long DataLoader::countLines() const {
     if (filePath.empty()) return -1;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return -1;
    }
    // Simplified line counting (can be slow)
    long long lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineCount;
    }
    return lineCount;
}

// NEW: Internal parsing helper based on config
bool DataLoader::parseLine(const std::string& line, Bar& bar) const {
    // Get format specifics from config (cache these in constructor for performance?)
    char delimiter = config.getNested<std::string>("/Data/CSV_Delimiter", ",")[0]; // Get first char
    int tsCol = config.getNested<int>("/Data/CSV_Timestamp_Col", 0);
    std::string tsFmt = config.getNested<std::string>("/Data/CSV_Timestamp_Format", "%Y%m%d %H%M%S%f");
    int bidCol = config.getNested<int>("/Data/CSV_Bid_Col", 1);
    int askCol = config.getNested<int>("/Data/CSV_Ask_Col", 2);
    int volCol = config.getNested<int>("/Data/CSV_Volume_Col", 3);
    // Add Open, High, Low, Close columns if needed later

    std::stringstream ss(line);
    std::string field;
    std::vector<std::string> fields;

    // Split line by delimiter
    while (std::getline(ss, field, delimiter)) {
        // Trim whitespace (optional, but good practice)
        // field.erase(0, field.find_first_not_of(" \t\n\r\f\v"));
        // field.erase(field.find_last_not_of(" \t\n\r\f\v") + 1);
        fields.push_back(field);
    }

    // Find the maximum required column index
    int maxIndex = 0;
    maxIndex = std::max({maxIndex, tsCol, bidCol, askCol, volCol});
    // Add indices for O,H,L,C if used

    if (static_cast<int>(fields.size()) <= maxIndex) {
        Utils::logMessage("DataLoader::parseLine Warning: Not enough fields (" + std::to_string(fields.size()) + ") in line for configured columns (max index " + std::to_string(maxIndex) + "). Line: " + line);
        return false;
    }

    try {
        // --- Parse Timestamp ---
        // Use the custom Utils::parseTimestamp which handles "%f" manually
        // Note: If Utils::parseTimestamp needs the format string, pass tsFmt to it.
        // Assuming Utils::parseTimestamp is hardcoded to "%Y%m%d %H%M%S%f" for now.
        // TODO: Modify Utils::parseTimestamp to accept a format string if needed.
        if (tsCol < 0 || tsCol >= fields.size()) throw std::out_of_range("Timestamp column index invalid");
        bar.timestamp = Utils::parseTimestamp(fields[tsCol]);

        // --- Parse Bid/Ask/Volume ---
        // Using std::from_chars for potentially better performance than stod/stoll
        auto parse_double = [&](int colIndex, double& out_val) {
             if (colIndex < 0 || colIndex >= fields.size()) throw std::out_of_range("Column index invalid for double");
             const std::string& s = fields[colIndex];
             auto result = std::from_chars(s.data(), s.data() + s.size(), out_val);
             // Check if parsing succeeded and consumed the whole string part intended
             if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
                 // Fallback or stricter error? Fallback to stod for flexibility?
                 // Let's throw for now if from_chars fails completely.
                  if (result.ec == std::errc::result_out_of_range) throw std::out_of_range("Double value out of range");
                  // Try stod as fallback? No, stick to stricter parsing.
                  throw std::invalid_argument("Invalid double format");
             }
        };
         auto parse_long = [&](int colIndex, long long& out_val) {
             if (colIndex < 0 || colIndex >= fields.size()) throw std::out_of_range("Column index invalid for long long");
              const std::string& s = fields[colIndex];
              auto result = std::from_chars(s.data(), s.data() + s.size(), out_val);
               if (result.ec != std::errc() || result.ptr != s.data() + s.size()) {
                    if (result.ec == std::errc::result_out_of_range) throw std::out_of_range("Long long value out of range");
                   throw std::invalid_argument("Invalid integer format");
               }
         };

        parse_double(bidCol, bar.bid);
        parse_double(askCol, bar.ask);
        parse_long(volCol, bar.volume);


        // --- Calculate OHLC ---
        // Currently calculated from Mid price, assuming M1 data
        // TODO: Add logic to parse O,H,L,C directly from CSV if columns are configured
        double mid = bar.midPrice();
         if (mid == 0.0 && (bar.bid != 0.0 || bar.ask != 0.0)) {
            Utils::logMessage("DataLoader::parseLine Warning: Bid/Ask result in zero mid-price. Line: " + line);
            return false; // Skip bars with zero mid unless bid/ask are also zero
         } else if (bar.bid == 0.0 && bar.ask == 0.0) {
              // Handle bars with zero bid/ask (e.g., use previous close, or skip)
               Utils::logMessage("DataLoader::parseLine Warning: Bid and Ask are both zero. Setting OHLC to zero. Line: " + line);
               // Set OHLC to 0, strategy should handle this if necessary
               bar.open = bar.high = bar.low = bar.close = 0.0;
               // Or return false to skip the bar entirely? Skipping might be safer.
               // return false;
         } else {
            bar.open = mid;
            bar.high = mid; // Approximation for M1
            bar.low = mid;  // Approximation for M1
            bar.close = mid;
         }


        return true; // Success

    } catch (const std::invalid_argument& e) {
        Utils::logMessage("DataLoader::parseLine Warning: Invalid numeric format: " + std::string(e.what()) + ". Line: " + line);
    } catch (const std::out_of_range& e) {
        Utils::logMessage("DataLoader::parseLine Warning: Numeric value out of range or invalid column index: " + std::string(e.what()) + ". Line: " + line);
    } catch (const std::exception& e) { // Catch other errors like timestamp parsing
        Utils::logMessage("DataLoader::parseLine Warning: Error processing line: " + std::string(e.what()) + ". Line: " + line);
    }

    return false; // Failure
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

    } // end while getline

    Utils::logMessage("DataLoader: Finished loading. Read " + std::to_string(data.size()) + " valid bars from " + std::to_string(currentLineNum) + " total lines processed.");
    return data;
}