// include/Position.h
#ifndef POSITION_H
#define POSITION_H

#include <string>
#include <chrono>
#include <cmath> // For std::abs

struct Position {
    std::string symbol = "";
    double size = 0.0;          // Positive for long, negative for short
    double entryPrice = 0.0;
    double lastValue = 0.0;
    double stopLossPrice = 0.0;
    double takeProfitPrice = 0.0; // <-- ADDED
    double pipPoint = 0.0001;
    std::chrono::system_clock::time_point entryTime{};

    // Calculates unrealized Profit/Loss for the position at the given current price
    double calculateUnrealizedPnL(double currentPrice) const {
        if (size == 0.0) return 0.0;
        return (currentPrice - entryPrice) * size;
    }

     // Calculates unrealized PnL in pips
    double calculateUnrealizedPips(double currentPrice) const {
        if (size == 0.0 || pipPoint == 0.0) return 0.0;
        double priceDiff = currentPrice - entryPrice;
        if (size < 0) { // Short position
             priceDiff = entryPrice - currentPrice;
        }
        return priceDiff / pipPoint;
    }
};

#endif // POSITION_H