// DataLoader.h - No major changes needed to the interface for now
#ifndef DATALOADER_H
#define DATALOADER_H

#include "Bar.h"
#include "Config.h" // Include Config to pass it
#include <string>
#include <vector>
#include <stdexcept>

class DataLoader {
private:
    // Store a reference or pointer to the config object
    const Config& config; // Use const reference
    std::string filePath; // Keep track of the specific file path if loaded

    // Helper to count lines (still potentially needed for partial load)
    long long countLines() const;

    // NEW: Internal parsing helper based on config
    bool parseLine(const std::string& line, Bar& bar) const; // Returns true on success

public:
    // Constructor now takes Config
    explicit DataLoader(const Config& cfg);

    // Load data function remains the same interface for now
    // But its implementation will use the config settings
    std::vector<Bar> loadData(bool usePartial = false, double partialPercent = 100.0);

    // --- Future Interface Ideas (Not implemented now) ---
    // virtual bool connectRealtime() { return false; } // For WebSocket etc.
    // virtual bool getNextBar(Bar& bar) { return false; } // For streaming
    // virtual bool isStreaming() const { return false; }
};

#endif // DATALOADER_H