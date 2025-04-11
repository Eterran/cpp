#include "RandomStrategy50.h"
#include "Broker.h"
#include "Utils.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>

// --- Constructor ---
RandomStrategy50::RandomStrategy50() :
    probDist(0.0, 1.0),
    directionDist(0, 1),
    outcomeGen(0, 1)  // 0 = loss, 1 = win
{
    // Seed RNG
    try {
        std::random_device rd;
        rng.seed(rd());
    } catch (...) {
        rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
}

// --- Initialization ---
void RandomStrategy50::init() {
    if (!config || !broker) {
        throw std::runtime_error("RandomStrategy50::init Error: Config or Broker not set");
    }
    
    // --- Load Parameters ---
    try {
        entryProbability = config->getNested<double>("/Strategy/ENTRY_PROBABILITY", 0.01);
        benchmarkFixedSize = config->getNested<double>("/Strategy/BENCHMARK_FIXED_SIZE", 1.0);
        debugMode = config->getNested<bool>("/Strategy/DEBUG_MODE", false);
    } catch (const std::exception& e) {
        Utils::logMessage("RandomStrategy50::init Warning: Error loading parameters: " + std::string(e.what()));
        // Use defaults already set in class definition
    }
    
    // Validate parameters
    entryProbability = std::clamp(entryProbability, 0.0, 1.0);
    if (benchmarkFixedSize <= 0) benchmarkFixedSize = 1.0;
    
    // --- Set up State ---
    startingAccountValue = broker->getCash();
    currentPendingOrderId = -1;
    tradeCount = 0;
    profitableTrades = 0;
    inPosition = false;
    
    // --- Log Initial Setup ---
    Utils::logMessage("--- RandomStrategy50 (Theoretical 50/50) Initialized ---");
    Utils::logMessage("Entry Probability per Bar: " + std::to_string(entryProbability * 100.0) + "%");
    Utils::logMessage("Benchmark Fixed Size: " + std::to_string(benchmarkFixedSize));
    Utils::logMessage("Starting Account Value: " + std::to_string(startingAccountValue));
    Utils::logMessage("-------------------------------------------------------");
}

// --- Per-Bar Logic ---
void RandomStrategy50::next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) {
    // Skip if we have a pending order
    if (currentPendingOrderId != -1) return;
    
    // --- Exit Logic (For positions) ---
    if (inPosition) {
        // Generate a truly random outcome (50% win, 50% loss)
        bool win = (outcomeGen(rng) == 1);
        
        // Determine the exit order type (opposite of entry direction)
        OrderType exitOrderType = (currentPosition.size > 0) ? OrderType::SELL : OrderType::BUY;
        OrderReason exitReason = win ? OrderReason::TAKE_PROFIT : OrderReason::STOP_LOSS;
        
        // Submit the exit order with the predetermined outcome
        currentPendingOrderId = broker->submitOrder(exitOrderType, exitReason, dataName, std::abs(currentPosition.size));
        
        if (currentPendingOrderId == -1) {
            Utils::logMessage("ERROR: Failed to submit exit order!");
        }
        return;
    }
    // --- Entry Logic ---
    else {
        double roll = probDist(rng);
        if (roll < entryProbability) {
            // Random direction (BUY or SELL)
            OrderType entryOrderType = (directionDist(rng) == 0) ? OrderType::BUY : OrderType::SELL;
            double desiredSize = benchmarkFixedSize;
            
            if (debugMode) Utils::logMessage("DEBUG: Random Entry Triggered (Roll: " + std::to_string(roll) + ")");
            
            currentPendingOrderId = broker->submitOrder(entryOrderType, OrderReason::ENTRY_SIGNAL, dataName, desiredSize);
            
            if (currentPendingOrderId == -1) {
                Utils::logMessage("ERROR: Failed to submit entry order!");
            }
            return;
        }
    }
}

// --- End of Backtest ---
void RandomStrategy50::stop() {
    double finalPortfolioValue = broker->getValue({{dataName, data->back().close}});
    Utils::logMessage("--- RandomStrategy50 (Theoretical 50/50) Finished ---");
    Utils::logMessage("========= TRADE SUMMARY =========");
    Utils::logMessage("Starting Portfolio Value: " + std::to_string(startingAccountValue));
    Utils::logMessage("Final Portfolio Value:    " + std::to_string(finalPortfolioValue));
    double netProfit = finalPortfolioValue - startingAccountValue;
    double netProfitPercent = (startingAccountValue > 0) ? (netProfit / startingAccountValue * 100.0) : 0.0;
    Utils::logMessage("Net Profit/Loss:          " + std::to_string(netProfit) + " (" + std::to_string(netProfitPercent) + "%)");
    Utils::logMessage("Total Trades Executed:    " + std::to_string(tradeCount));
    Utils::logMessage("Profitable Trades:        " + std::to_string(profitableTrades));
    double winRate = (tradeCount > 0) ? (static_cast<double>(profitableTrades) / tradeCount * 100.0) : 0.0;
    Utils::logMessage("Win Rate:                 " + std::to_string(winRate) + "%");
    Utils::logMessage("=================================");
}

// --- Order Notification Handler ---
void RandomStrategy50::notifyOrder(const Order& order) {
    if (order.id == currentPendingOrderId) {
        currentPendingOrderId = -1;
    }
    
    if (order.status == OrderStatus::FILLED) {
        // --- Entry Fill ---
        if (order.reason == OrderReason::ENTRY_SIGNAL && !inPosition) {
            tradeCount++;
            inPosition = true;
            currentPosition.symbol = order.symbol;
            currentPosition.size = (order.type == OrderType::BUY) ? order.filledSize : -order.filledSize;
            currentPosition.entryPrice = order.filledPrice;
            
            Utils::logMessage(">>> ENTRY EXECUTED <<< | Trade #" + std::to_string(tradeCount) 
                           + " | " + ((order.type == OrderType::BUY) ? "BUY" : "SELL") + " " 
                           + std::to_string(order.filledSize) + " @ " + std::to_string(order.filledPrice));
        }
        // --- Exit Fill ---
        else if (inPosition && order.symbol == currentPosition.symbol &&
                ((order.type == OrderType::SELL && currentPosition.size > 0) ||
                 (order.type == OrderType::BUY && currentPosition.size < 0))) {
            
            // Update profitableTrades counter based on the predefined outcome
            if (order.reason == OrderReason::TAKE_PROFIT) {
                profitableTrades++;
            }
            
            std::string exitReasonStr = (order.reason == OrderReason::TAKE_PROFIT) ? "TP" : "SL";
            
            Utils::logMessage("<<< EXIT EXECUTED (" + exitReasonStr + ") >>> | Trade #" + std::to_string(tradeCount)
                           + " | " + ((order.type == OrderType::BUY) ? "BUY" : "SELL") + " " 
                           + std::to_string(order.filledSize) + " @ " + std::to_string(order.filledPrice)
                           + (order.reason == OrderReason::TAKE_PROFIT ? " (PROFIT)" : " (LOSS)"));
                           
            inPosition = false;
            currentPosition = Position();
        }
    }
    else if (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN || order.status == OrderStatus::CANCELLED) {
        Utils::logMessage("--- ORDER ISSUE --- | ID: " + std::to_string(order.id) 
                       + " Status: " + std::to_string(static_cast<int>(order.status)));
    }
}
