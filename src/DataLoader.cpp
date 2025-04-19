// DataLoader.cpp
#include "DataLoader.h"
#include "Utils.h"
#include "Config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <vector>
#include <future>
#include <charconv>
#include "CSVDataSource.h"
#include "APIDataSource.h"
#include "json.hpp"
#include "JSONParserStep.h"
#include "CSVParserStep.h"

// Constructor takes Config reference
DataLoader::DataLoader(const Config& cfg) :
    config(cfg)
{
    // Choose data source based on config
    std::string input = config.getNested<std::string>("/Data/INPUT_SOURCE", "csv");
    if (input == "api") {
        std::string url = config.getNested<std::string>("/Data/API_URL", "");
        dataSource = std::make_unique<APIDataSource>(url, config);
    } else {
        std::string path = config.getNested<std::string>("/Data/INPUT_CSV_PATH", "");
        char delim = config.getNested<std::string>("/Data/CSV_Delimiter", ",")[0];
        bool skipHeader = config.getNested<bool>("/Data/CSV_Has_Header", false);
        dataSource = std::make_unique<CSVDataSource>(path, delim, skipHeader);
        Utils::logMessage("DataLoader: Initialized with " + path + " dataset.");
    }
    initParserSteps();
}

// Helper to count lines (implementation remains the same)
long long DataLoader::countLines() const {
    return dataSource->count();
}

// Initialize parser pipeline
void DataLoader::initParserSteps() {
    parserSteps.clear();
    // CSV pipeline
    auto csvSpecs = config.getColumnSpecs("/Data/CSV_Columns");
    std::string csvTsFmt = config.getNested<std::string>("/Data/CSV_Timestamp_Format", "%Y-%m-%d %H:%M:%S");
    char csvDelim = config.getNested<std::string>("/Data/CSV_Delimiter", ",")[0];
    parserSteps.emplace_back(
        std::make_unique<CSVParserStep>(std::move(csvSpecs), csvTsFmt, csvDelim)
    );
    // JSON/API pipeline
    auto apiSpecs = config.getColumnSpecs("/Data/API_Columns");
    std::string apiTsFmt = config.getNested<std::string>("/Data/API_Timestamp_Format", "%Y-%m-%dT%H:%M:%S");
    parserSteps.emplace_back(
        std::make_unique<JSONParserStep>(std::move(apiSpecs), apiTsFmt)
    );
}

// Delegates to parser steps
bool DataLoader::parseLine(const std::string& line, Bar& bar) const {
    for (const auto& step : parserSteps) {
        if (step->parse(line, bar)) return true;
    }
    return false;
}

// Load data implementation uses the internal parseLine helper
std::vector<Bar> DataLoader::loadData(bool usePartial, double partialPercent) {
    if (!dataSource->open()) {
        Utils::logMessage("DataLoader Error: Could not open data source.");
        return {};
    }
    Utils::logMessage("DataLoader: Starting read phase.");

    // Read records into memory
    std::vector<std::string> lines;
    long long total = countLines();
    long long linesToRead = total;
    if (usePartial && partialPercent > 0 && partialPercent < 100.0 && total > 0) {
        linesToRead = static_cast<long long>(std::ceil(total * partialPercent / 100.0));
    }
    if (linesToRead > 0 && total > 0) lines.reserve(std::min(linesToRead, total));

    std::string line;
    long long count = 0;
    while (dataSource->getNext(line) && (linesToRead <= 0 || count < linesToRead)) {
        ++count;
        if (line.empty() || line[0] == '#') continue;
        lines.push_back(line);
    }
    dataSource->close();
    Utils::logMessage("DataLoader: Completed read phase. Collected " + std::to_string(lines.size()) + " records.");

    // Parse phase: sequential or parallel
    std::vector<Bar> data;
    int numThreads = config.getNested<int>("/Data/Threads", 2);
    if (numThreads > 1 && lines.size() > 0) {
        Utils::logMessage("DataLoader: Parsing in parallel using " + std::to_string(numThreads) + " threads.");
        std::vector<std::future<std::vector<Bar>>> futures;
        size_t totalLines = lines.size();
        size_t chunk = totalLines / numThreads;
        size_t start = 0;
        for (int t = 0; t < numThreads; ++t) {
            size_t end = (t == numThreads - 1) ? totalLines : start + chunk;
            futures.emplace_back(std::async(std::launch::async, [this, &lines, start, end] {
                std::vector<Bar> local;
                local.reserve(end - start);
                for (size_t i = start; i < end; ++i) {
                    Bar bar;
                    if (parseLine(lines[i], bar)) local.push_back(bar);
                }
                return local;
            }));
            start = end;
        }
        for (auto& fut : futures) {
            auto local = fut.get();
            data.insert(data.end(), local.begin(), local.end());
        }
    } else {
        Utils::logMessage("DataLoader: Parsing sequentially.");
        data.reserve(lines.size());
        for (const auto& rec : lines) {
            Bar bar;
            if (parseLine(rec, bar)) data.push_back(bar);
        }
    }
    Utils::logMessage("DataLoader: Finished parse phase. Produced " + std::to_string(data.size()) + " bars.");
    return data;
}