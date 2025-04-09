#ifndef BAR_H
#define BAR_H

#include <chrono>
#include <string> // Often useful, e.g., if adding a toString() method later

// Represents a single time period's data (OHLCV + Bid/Ask)
struct Bar {
    std::chrono::system_clock::time_point timestamp{}; // Default initialize
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    long long volume = 0; // Use long long for potentially large volumes

    // Helper function to get the mid-price (as used in Python preprocessing)
    double midPrice() const {
        // Avoid division by zero if bid/ask somehow are zero (unlikely but safe)
        if (bid != 0.0 || ask != 0.0) {
             return (bid + ask) / 2.0;
        }
        return 0.0; // Or handle as an error/NaN depending on requirements
    }
};

#endif // BAR_H