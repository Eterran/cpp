#ifndef TRADINGMETRICS_H
#define TRADINGMETRICS_H

#include <vector>
#include <string>
#include <chrono>
#include "Order.h"

class TradingMetrics {
private:
    double startingValue;
    double highestValue;
    double maxDrawdown;
    int tradeCount;
    int profitableTrades;
    double totalCommission;
    double previousValue;
    std::vector<double> returns;
    size_t totalBars;

public:
    TradingMetrics(double initialValue);
    
    // Methods to track metrics
    void recordTrade(bool profitable);
    void recordCommission(double commission);
    void updatePortfolioValue(double currentValue);
    void recordReturn(double periodReturn);
    void setTotalBars(size_t bars) { totalBars = bars; }
    
    // Metric calculation methods
    double getPreviousValue() const { return previousValue; }
    int getTotalTrades() const { return tradeCount; }
    int getProfitableTrades() const { return profitableTrades; }
    double getWinRate() const;
    double getMaxDrawdown() const { return maxDrawdown; }
    double getSharpeRatio() const;
    double getTotalCommission() const { return totalCommission; }
    double getTradingFrequency() const;
    
    // Generate summary as string
    std::string generateSummaryReport(double finalValue, const std::string& strategyName) const;
};

#endif // TRADINGMETRICS_H
