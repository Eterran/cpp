#ifndef BAR_H
#define BAR_H

#include <chrono>
#include <string> 
#include <array>
#include <vector>
#include <variant>

// Represents a single time period's data (OHLCV + Bid/Ask)
struct Bar {
    std::chrono::system_clock::time_point timestamp{}; // Default initialize

    std::vector<std::string> columnNames;

    std::vector<double> columns;
};

#endif // BAR_H