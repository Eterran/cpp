// DataLoader.h
#ifndef DATALOADER_H
#define DATALOADER_H

#include "Bar.h"
#include "Config.h"
#include "DataSource.h"
#include "CSVDataSource.h"
#include "APIDataSource.h"
#include "ParserStep.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

class DataLoader {
private:
    const Config& config;
    std::unique_ptr<IDataSource> dataSource;  // Abstract data source

    // Helper to count lines
    long long countLines() const;

    std::vector<std::unique_ptr<ParserStep>> parserSteps;  // Parsing pipeline
    void initParserSteps();

    bool parseLine(const std::string& line, Bar& bar) const; // Delegates to parserSteps

public:
    explicit DataLoader(const Config& cfg);
    std::vector<Bar> loadData(bool usePartial = false, double partialPercent = 100.0);

    // --- Future Interface Ideas (Not implemented now) ---
    // virtual bool connectRealtime() { return false; } // For WebSocket etc.
    // virtual bool getNextBar(Bar& bar) { return false; } // For streaming
    // virtual bool isStreaming() const { return false; }
};

#endif // DATALOADER_H