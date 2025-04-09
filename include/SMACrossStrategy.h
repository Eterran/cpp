// SMACrossStrategy.h
#ifndef SMACROSSSTRATEGY_H
#define SMACROSSSTRATEGY_H

#include "Strategy.h"
#include "SMAIndicator.h"
#include "Position.h" // Include Position to store current trade details
#include "Config.h"   // Include full Config definition now
#include "Utils.h"    // For logging, pip point calc


class SmaCrossStrategy : public Strategy {
private:
    // --- Parameters (Loaded from Config in init) ---
    int smaPeriod = 200; // Default value
    std::string positionType = "fixed";
    double fixedSize = 100;
    double cashPercent = 2.0;
    double riskPercent = 1.0;
    
    double stopLossPips = 50.0;
    bool stopLossEnabled = true;
    bool bankruptcyProtection = true;
    double forceExitPercent = -50.0; // Percentage account drawdown

    bool takeProfitEnabled = true; 
    double takeProfitPips = 100.0; 

    bool debugMode = false;

    // --- Strategy State ---
    std::string name = "SmaCrossStrategy";
    SMAIndicator smaIndicator;
    double pipPoint = 0.0001;
    int currentPendingOrderId = -1; // Track the ID of a pending entry/exit order
    int tradeCount = 0;
    int profitableTrades = 0;
    Position currentPosition;      // Stores details when in a trade
    bool inPosition = false;       // Flag indicating if holding a position
    bool bankrupt = false;         // Flag for bankruptcy protection
    bool priceAboveSma = false;    // Current relationship
    bool prevPriceAboveSma = false;// Previous bar's relationship
    double startingAccountValue = 0.0; // Store initial value for drawdown check

    // --- Helper Methods ---
    // Calculates desired position size based on configuration
    double calculateSize(double currentPrice, double accountValue);
    // Logs strategy-specific debug info
    void logDebug(const Bar& bar, double currentSma, bool crossoverAbove, bool crossoverBelow, double accountValue);

public:
    // Constructor initializes indicator with a default period (can be overridden in init)
    SmaCrossStrategy();
    std::string getName();
    // --- Overridden Lifecycle Methods ---
    void init() override;
    void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // SMACROSSSTRATEGY_H