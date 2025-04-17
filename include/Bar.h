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
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    long long volume = 0;

    double priceChange() const {
        return getClose() - open;
    }

    std::vector<std::variant<double, int, long long, std::string>> extraColumns;

    double getOpen() const {
        if(open != 0.0) {
            return open;
        } else return close;
    }

    double getHigh() const {
        if(high != 0.0) {
            return high;
        } else return close;
    }

    double getLow() const {
        if(low != 0.0) {
            return low;
        } else return close;
    }

    double getClose() const {
        if(close != 0.0) {
            return close;
        } else return 0.0;
    }

    double getBid() const {
        if(bid != 0.0) {
            return bid;
        } else return close;
    }

    double getAsk() const {
        if(ask != 0.0) {
            return ask;
        } else return close;
    }

    double midPrice() const {
        if (bid != 0.0 || ask != 0.0) {
            return (bid + ask) / 2.0;
        }
        return 0.0;
    }
};

#endif // BAR_H