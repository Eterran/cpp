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
    double pointValue = 1;
    double stopLoss = 0.0;
    double takeProfit = 0.0;
    std::chrono::system_clock::time_point entryTime{};

    // Calculates unrealized Profit/Loss for the position at the given current price
    double calculateUnrealizedPnL(double currentPrice) const {
        if (size == 0.0) return 0.0;
        // For long positions: (currentPrice - entryPrice) * size is positive when price rises
        // For short positions: (currentPrice - entryPrice) * size is negative when price rises
        // This correctly accounts for both directions
        return (currentPrice - entryPrice) * size;
    }

     // Calculates unrealized PnL in pips
    double calculateUnrealizedPoints(double currentPrice) const {
        if (size == 0.0 || pointValue == 0.0) return 0.0;
        double priceDiff = currentPrice - entryPrice;
        if (size < 0) { // Short position
            priceDiff = entryPrice - currentPrice;
        }
        return priceDiff / pointValue;
    }
};

#endif // POSITION_H