#include "TradingMetrics.h"
#include "Utils.h"
#include <cmath>
#include <sstream>
#include <iomanip>

TradingMetrics::TradingMetrics(double initialValue) :
    startingValue(initialValue),
    highestValue(initialValue),
    maxDrawdown(0.0),
    tradeCount(0),
    profitableTrades(0),
    totalCommission(0.0),
    totalBars(0)
{
}

void TradingMetrics::recordTrade(bool profitable) {
    tradeCount++;
    if (profitable) profitableTrades++;
}

void TradingMetrics::recordCommission(double commission) {
    totalCommission += commission;
}

// returns updated
void TradingMetrics::updatePortfolioValue(double currentValue) {
    // Update highest value and drawdown
    previousValue = currentValue;
    if (currentValue > highestValue) {
        highestValue = currentValue;
    } else if (highestValue > 0) {
        double currentDrawdown;

        currentDrawdown = (highestValue - currentValue) / highestValue * 100.0;

        if (currentDrawdown > maxDrawdown) {
            maxDrawdown = currentDrawdown;
        }
    }
}

void TradingMetrics::recordReturn(double periodReturn) {
    returns.push_back(periodReturn);
}

double TradingMetrics::getWinRate() const {
    return (tradeCount > 0) ? (static_cast<double>(profitableTrades) / tradeCount * 100.0) : 0.0;
}

// TODO: 365 or 252 trading days based on instrument
double TradingMetrics::getSharpeRatio() const {
    if (returns.empty()) return 0.0;

    // Calculate average return
    double sum = 0.0;
    for (double ret : returns) {
        sum += ret;
    }
    double meanReturn = sum / returns.size();

    // Calculate standard deviation
    double sqSum = 0.0;
    for (double ret : returns) {
        sqSum += (ret - meanReturn) * (ret - meanReturn);
    }
    double stdDev = std::sqrt(sqSum / returns.size());

    // Annualized Sharpe Ratio (assuming hourly returns and 365 trading days)
    if (stdDev <= 0 || returns.size() < 2) {
        Utils::logMessage("Sharpe Ratio calculation: Insufficient data or zero standard deviation. Returns size: " + std::to_string(returns.size()) + ", StdDev: " + std::to_string(stdDev));
        return 0.0;
    }
    // double riskFreeRate = ...; // Hourly risk-free rate
    // double excessMeanReturn = meanReturn - riskFreeRate;
    // double sharpeRatio = (excessMeanReturn / stdDev) * std::sqrt(365*24);
    double sharpeRatio = (meanReturn / stdDev) * std::sqrt(365*24);
    Utils::logMessage("Sharpe Ratio calculation: Mean Return: " + std::to_string(meanReturn) + ", StdDev: " + std::to_string(stdDev) + ", Sharpe: " + std::to_string(sharpeRatio));
    return sharpeRatio;
}

double TradingMetrics::getTradingFrequency() const {
    return (totalBars > 0) ? (static_cast<double>(tradeCount) / totalBars * 100.0) : 0.0;
}

std::string TradingMetrics::generateSummaryReport(double finalValue, const std::string& strategyName) const {
    std::stringstream report;
    report << std::fixed << std::setprecision(2);

    double netProfit = finalValue - startingValue;
    double netProfitPercent = (startingValue > 0) ? (netProfit / startingValue * 100.0) : 0.0;

    report << "--- " << strategyName << " Finished ---\n";
    report << "========== TRADE SUMMARY ===========\n";
    report << "Starting Portfolio Value: " << startingValue << "\n";
    report << "Final Portfolio Value:    " << finalValue << "\n";
    report << "Net Profit/Loss:          " << netProfit << " (" << netProfitPercent << "%)\n";
    report << "Total Trades Executed:    " << tradeCount << "\n";
    report << "Profitable Trades:        " << profitableTrades << "\n";
    report << "Win Rate:                 " << getWinRate() << "%\n";
    report << "Total Commission Fees:    " << totalCommission << "\n";
    report << "Max Drawdown:             " << maxDrawdown << "%\n";
    report << "Sharpe Ratio:             " << getSharpeRatio() << "\n";
    report << "Trading Frequency:        " << getTradingFrequency() << " trades per 100 bars\n";
    report << "===================================";

    return report.str();
}
