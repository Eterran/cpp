// src/RandomStrategy.cpp
#include "RandomStrategy.h"
#include "Broker.h"
#include "Utils.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

// --- Constructor ---
RandomStrategy::RandomStrategy() :
    probDist(0.0, 1.0),
    directionDist(0, 1)
{
    // Seed RNG (same as before)
    try {
        std::random_device rd;
        rng.seed(rd());
        // Utils::logMessage("RandomStrategy: RNG seeded using random_device."); // Less verbose
    } catch (...) {
        // Utils::logMessage("RandomStrategy Warning: random_device failed. Seeding RNG using system clock.");
        rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
}

// --- Initialization ---
void RandomStrategy::init() {
    if (!config || !broker) {
        throw std::runtime_error("RandomStrategy::init Error: Config pointer or Broker not set.");
    }

    // --- Load Parameters ---
    try {
        entryProbability = config->getNested<double>("/Strategy/ENTRY_PROBABILITY", 0.01);
        slTpPips = config->getNested<double>("/Strategy/SL_TP_PIPS", 100.0); // Default benchmark 100 pips
        benchmarkFixedSize = config->getNested<double>("/Strategy/BENCHMARK_FIXED_SIZE", 1.0); // Default benchmark size 1 unit

        bankruptcyProtection = config->getNested<bool>("/Strategy/BANKRUPTCY_PROTECTION", true);
        forceExitPercent = config->getNested<double>("/Strategy/FORCE_EXIT_PERCENT", -50.0);
        debugMode = config->getNested<bool>("/Strategy/DEBUG_MODE", false);

    } catch (const std::exception& e) {
        Utils::logMessage("RandomStrategy::init Warning: Error loading config parameter: " + std::string(e.what()) + ". Using defaults.");
        entryProbability = 0.01;
        slTpPips = 100.0;
        benchmarkFixedSize = 1.0;
        bankruptcyProtection = true;
        forceExitPercent = -50.0;
        debugMode = false;
    }

    // Validate parameters
    entryProbability = std::clamp(entryProbability, 0.0, 1.0);
    if (slTpPips <= 0) slTpPips = 100.0;
    if (benchmarkFixedSize <= 0) benchmarkFixedSize = 1.0;


    // --- Set up State ---
    pipPoint = Utils::calculatePipPoint(dataName);
    broker->setPipPoint(dataName, pipPoint);
    startingAccountValue = broker->getCash();
    currentPendingOrderId = -1;
    tradeCount = 0;
    profitableTrades = 0;
    inPosition = false;
    bankrupt = false;

    // --- Log Initial Setup ---
    Utils::logMessage("--- Random Benchmark Strategy Initialized ---");
    Utils::logMessage("Data Name: " + dataName + ", Pip Point: " + std::to_string(pipPoint));
    Utils::logMessage("Entry Probability per Bar: " + std::to_string(entryProbability * 100.0) + "%");
    Utils::logMessage("SL/TP Distance (Pips): " + std::to_string(slTpPips));
    Utils::logMessage("Benchmark Fixed Size: " + std::to_string(benchmarkFixedSize));
    Utils::logMessage("Bankruptcy Protection: " + std::string(bankruptcyProtection ? "Yes" : "No"));
    Utils::logMessage("Starting Account Value (Cash): " + std::to_string(startingAccountValue));
    Utils::logMessage("-------------------------------------------");
}

// --- Per-Bar Logic ---
void RandomStrategy::next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) {
    // --- Guard Clauses ---
    if (bankrupt || currentPendingOrderId != -1) return;

    double accountValue = broker->getValue(currentPrices);
    // Use Bid/Ask for more realistic SL/TP checks if available
    double currentCheckPrice = currentBar.close; // Default to close if bid/ask zero
     if(inPosition && currentPosition.size > 0 && currentBar.bid > 0) { // Long position, check against Bid
         currentCheckPrice = currentBar.bid;
     } else if (inPosition && currentPosition.size < 0 && currentBar.ask > 0) { // Short position, check against Ask
         currentCheckPrice = currentBar.ask;
     } else if (currentBar.midPrice() != 0) { // Fallback to mid if bid/ask missing but calculable
          currentCheckPrice = currentBar.midPrice();
     }
     // else stick with close price


    // --- Position Management (Check SL/TP Hits) ---
    if (inPosition) {
        // --- Bankruptcy Protection Check ---
        if (bankruptcyProtection) {
            double drawdownPercent = (accountValue / startingAccountValue - 1.0) * 100.0;
            if (accountValue <= 1.0 || drawdownPercent <= forceExitPercent) {
                 Utils::logMessage("!!! BANKRUPTCY PROTECTION TRIGGERED !!!");
                 OrderType exitOrderType = (currentPosition.size > 0) ? OrderType::SELL : OrderType::BUY;
                 currentPendingOrderId = broker->submitOrder(exitOrderType, OrderReason::BANKRUPTCY_PROTECTION, dataName, std::abs(currentPosition.size));
                 if (currentPendingOrderId != -1) bankrupt = true; else bankrupt = true; // Set bankrupt anyway
                return;
            }
        }

        bool exitTriggered = false;
        OrderReason exitReason = OrderReason::MANUAL_CLOSE;
        OrderType exitOrderType = (currentPosition.size > 0) ? OrderType::SELL : OrderType::BUY;

        // --- Take Profit Check ---
        if (currentPosition.takeProfitPrice > 0) {
            bool tpHit = false;
            if (currentPosition.size > 0 && currentCheckPrice >= currentPosition.takeProfitPrice) tpHit = true; // Long TP
            else if (currentPosition.size < 0 && currentCheckPrice <= currentPosition.takeProfitPrice) tpHit = true; // Short TP

            if (tpHit) {
                 if(debugMode) Utils::logMessage("DEBUG: Take Profit Triggered: Price " + std::to_string(currentCheckPrice) + " vs TP " + std::to_string(currentPosition.takeProfitPrice));
                 exitTriggered = true;
                 exitReason = OrderReason::TAKE_PROFIT;
            }
        }

        // --- Stop Loss Check ---
        if (!exitTriggered && currentPosition.stopLossPrice > 0) {
             bool slHit = false;
             if (currentPosition.size > 0 && currentCheckPrice <= currentPosition.stopLossPrice) slHit = true; // Long SL
             else if (currentPosition.size < 0 && currentCheckPrice >= currentPosition.stopLossPrice) slHit = true; // Short SL

             if (slHit) {
                 if(debugMode) Utils::logMessage("DEBUG: Stop Loss Triggered: Price " + std::to_string(currentCheckPrice) + " vs SL " + std::to_string(currentPosition.stopLossPrice));
                 exitTriggered = true;
                 exitReason = OrderReason::STOP_LOSS;
             }
        }

        // --- Submit Exit Order ---
        if (exitTriggered) {
            currentPendingOrderId = broker->submitOrder(exitOrderType, exitReason, dataName, std::abs(currentPosition.size));
            if (currentPendingOrderId == -1) Utils::logMessage("  ERROR: Failed to submit exit order!");
            return;
        }
    }
    // --- Entry Logic ---
    else { // Not in position
        double roll = probDist(rng);
        if (roll < entryProbability) {
            // No price/indicator checks needed

            OrderType entryOrderType = (directionDist(rng) == 0) ? OrderType::BUY : OrderType::SELL;
            double desiredSize = benchmarkFixedSize; // Use the fixed benchmark size

            if (desiredSize <= 0) return; // Should not happen with default > 0

            if(debugMode) Utils::logMessage("DEBUG: Random Entry Triggered (Roll: " + std::to_string(roll) + ")");
            Utils::logMessage("  Attempting " + std::string(entryOrderType == OrderType::BUY ? "BUY" : "SELL")
                            + " order for " + std::to_string(desiredSize) + " units of " + dataName);

            currentPendingOrderId = broker->submitOrder(entryOrderType, OrderReason::ENTRY_SIGNAL, dataName, desiredSize);
            if (currentPendingOrderId == -1) Utils::logMessage("  ERROR: Failed to submit entry order!");
            return;
        }
    }
}

// --- End of Backtest ---
void RandomStrategy::stop() {
    // Summary logic remains the same
    double finalPortfolioValue = broker->getValue({{dataName, data->back().close}});
    Utils::logMessage("--- Random Benchmark Strategy Finished ---");
    if (bankrupt) Utils::logMessage("Trading stopped early due to Bankruptcy Protection trigger.");
    Utils::logMessage("========= TRADE SUMMARY =========");
    Utils::logMessage("Starting Portfolio Value: " + std::to_string(startingAccountValue));
    Utils::logMessage("Final Portfolio Value:    " + std::to_string(finalPortfolioValue));
    double netProfit = finalPortfolioValue - startingAccountValue;
    double netProfitPercent = (startingAccountValue > 0) ? (netProfit / startingAccountValue * 100.0) : 0.0;
    Utils::logMessage("Net Profit/Loss:          " + std::to_string(netProfit) + " (" + std::to_string(netProfitPercent) + "%)");
    Utils::logMessage("Total Trades Executed:    " + std::to_string(tradeCount));
    Utils::logMessage("Profitable Trades:      " + std::to_string(profitableTrades));
    double winRate = (tradeCount > 0) ? (static_cast<double>(profitableTrades) / tradeCount * 100.0) : 0.0;
    Utils::logMessage("Win Rate:               " + std::to_string(winRate) + "%");
    Utils::logMessage("=================================");
}

// --- Order Notification Handler ---
void RandomStrategy::notifyOrder(const Order& order) {
     // Logic remains largely the same as the previous RandomStrategy version
     // Ensure SL/TP prices are calculated correctly based on slTpPips parameter
     // Ensure PnL calculation and profitableTrades counter work correctly.

    if (order.id == currentPendingOrderId) {
        currentPendingOrderId = -1;
    }

    if (debugMode) { // Optional: Log all notifications in debug mode
        std::stringstream dss;
         dss << std::fixed << std::setprecision(5);
         dss << "DEBUG Notify | ID:" << order.id << " Type:" << static_cast<int>(order.type)
             << " Status:" << static_cast<int>(order.status) << " Reason:" << static_cast<int>(order.reason)
             << " ReqSz:" << order.requestedSize << " FillSz:" << order.filledSize << " FillPx:" << order.filledPrice;
         Utils::logMessage(dss.str());
    }

    if (order.status == OrderStatus::FILLED) {
        // Entry Fill
        if (order.reason == OrderReason::ENTRY_SIGNAL && !inPosition) {
            tradeCount++;
            inPosition = true;
            currentPosition.symbol = order.symbol;
            currentPosition.size = (order.type == OrderType::BUY) ? order.filledSize : -order.filledSize;
            currentPosition.entryPrice = order.filledPrice;
            currentPosition.entryTime = order.executionTime;
            currentPosition.pipPoint = pipPoint;
            currentPosition.lastValue = std::abs(currentPosition.size * currentPosition.entryPrice);

            double distance = slTpPips * pipPoint; // Use the benchmark pips value
            if (currentPosition.size > 0) { // Long
                currentPosition.stopLossPrice = order.filledPrice - distance;
                currentPosition.takeProfitPrice = order.filledPrice + distance;
            } else { // Short
                currentPosition.stopLossPrice = order.filledPrice + distance;
                currentPosition.takeProfitPrice = order.filledPrice - distance;
            }
             Utils::logMessage(">>> ENTRY EXECUTED <<< | Trade #" + std::to_string(tradeCount)
                           + " | " + ((order.type == OrderType::BUY)?"BUY":"SELL") + " " + std::to_string(order.filledSize)
                           + " @ " + std::to_string(order.filledPrice)
                           + " | SL: " + std::to_string(currentPosition.stopLossPrice)
                           + " | TP: " + std::to_string(currentPosition.takeProfitPrice));

        }
        // Exit Fill
        else if (inPosition && order.symbol == currentPosition.symbol &&
                 ((order.type == OrderType::SELL && currentPosition.size > 0) ||
                  (order.type == OrderType::BUY && currentPosition.size < 0)) )
        {
             if (order.filledSize >= std::abs(currentPosition.size) - 1e-9) {
                 double pnl = (order.filledPrice - currentPosition.entryPrice) * currentPosition.size;
                 // double pips = pnl / (std::abs(currentPosition.size) * pipPoint); // Pips calculation can be tricky

                 if (pnl > 0) {
                     profitableTrades++;
                 }

                  std::string exitReasonStr = "OTHER";
                  if(order.reason == OrderReason::STOP_LOSS) exitReasonStr = "SL";
                  else if(order.reason == OrderReason::TAKE_PROFIT) exitReasonStr = "TP";
                  else if(order.reason == OrderReason::BANKRUPTCY_PROTECTION) exitReasonStr = "BANKRUPTCY";

                  Utils::logMessage("<<< EXIT EXECUTED ("+exitReasonStr+") >>> | Trade #" + std::to_string(tradeCount)
                                 + " | " + ((order.type == OrderType::BUY)?"BUY":"SELL") + " " + std::to_string(order.filledSize)
                                 + " @ " + std::to_string(order.filledPrice)
                                 + " | P/L: " + std::to_string(pnl) + (pnl>0?" (PROFIT)":" (LOSS)"));

                 inPosition = false;
                 currentPosition = Position();
             } else { /* Handle partial fill notification if needed */ }
        } else { /* Handle unexpected fills */ }

    } else if (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN || order.status == OrderStatus::CANCELLED) {
         Utils::logMessage("--- ORDER ISSUE --- | ID: " + std::to_string(order.id) + " Status: " + std::to_string(static_cast<int>(order.status)) + " Reason: " + std::to_string(static_cast<int>(order.reason)) );
          if (inPosition && ((order.type == OrderType::SELL && currentPosition.size > 0) || (order.type == OrderType::BUY && currentPosition.size < 0))) {
               Utils::logMessage("CRITICAL WARNING: Exit Order Failed! Position remains open!");
               if (order.reason == OrderReason::BANKRUPTCY_PROTECTION) bankrupt = true;
          }
    }
}

// --- calculateSize method is removed ---