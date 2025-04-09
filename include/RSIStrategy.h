// RsiStrategy.h
#ifndef RSISTRATEGY_H
#define RSISTRATEGY_H

#include "Strategy.h"
#include "RSIIndicator.h" // Include necessary indicator(s)
#include "Position.h"
#include "Config.h"
#include "Utils.h"

class RsiStrategy : public Strategy {
private:
    // --- Parameters ---
    int rsiPeriod = 14;
    double rsiOversold = 30.0;
    double rsiOverbought = 70.0;
    // ... other strategy-specific params (SL, TP, sizing etc.)

    // --- Indicators ---
    RSIIndicator rsiIndicator;

    // --- State ---
    // ... strategy state (inPosition, pendingOrderId, trade counters, etc.) ...
    Position currentPosition;
    bool inPosition = false;
    int currentPendingOrderId = -1;
    // ... other state vars ...

    // --- Helpers ---
    double calculateSize(/*...*/); // Sizing logic might be reused or specific

public:
    RsiStrategy(); // Constructor

    // --- Overrides ---
    void init() override;
    void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // RSISTRATEGY_H