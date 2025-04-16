#include "RandomStrategy.h"
#include "Broker.h"
#include "Utils.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>

// --- Constructor ---
RandomStrategy::RandomStrategy() :
    probDist(0.0, 1.0),
    directionDist(0, 1)
{
    try {
        std::random_device rd;
        rng.seed(rd());
    } catch (...) {
        rng.seed(static_cast<unsigned int>(
            std::chrono::system_clock::now().time_since_epoch().count()));
    }
}

// --- Initialization ---
void RandomStrategy::init() {
    if (!config || !broker) {
        throw std::runtime_error("RandomStrategy::init Error: Config or Broker not set");
    }

    // --- Load Parameters ---
    try {
        entryProbability = config->getNested<double>("/Strategy/ENTRY_PROBABILITY", 0.01);
        benchmarkFixedSize = config->getNested<double>("/Strategy/BENCHMARK_FIXED_SIZE", 1.0);
        debugMode = config->getNested<bool>("/Strategy/DEBUG_MODE", false);
        one_trade = config->getNested<bool>("/Strategy/ONE_TRADE", true);
    } catch (const std::exception& e) {
        Utils::logMessage("RandomStrategy::init Warning: Error loading parameters: " + std::string(e.what()));
        // Use defaults already set in class definition
    }

    // Validate parameters
    entryProbability = std::clamp(entryProbability, 0.0, 1.0);
    if (benchmarkFixedSize <= 0) benchmarkFixedSize = 1.0;

    // --- Set up State ---
    currentOrderId = -1;
    inPosition = false;
    taken_trade = false;
    double startingAccountValue = broker->getStartingCash();

    // Convert entry probability from percentage to decimal if needed
    if (entryProbability > 1.0) {
        entryProbability = entryProbability / 100.0;
        Utils::logMessage("Converting entry probability from percentage to decimal: " + std::to_string(entryProbability));
    }

    // Initialize metrics tracker
    metrics = std::make_unique<TradingMetrics>(startingAccountValue);
    if (data) {
        metrics->setTotalBars(data->size());
    }

    // --- Log Initial Setup ---
    Utils::logMessage("--- RandomStrategy (Theoretical 50/50) Initialized ---");
    Utils::logMessage("Entry Probability per Bar: " + std::to_string(entryProbability * 100.0) + "%");
    Utils::logMessage("Benchmark Fixed Size: " + std::to_string(benchmarkFixedSize));
    Utils::logMessage("Starting Account Value: " + std::to_string(startingAccountValue));
    Utils::logMessage("-------------------------------------------------------");
}

// --- Per-Bar Logic ---
void RandomStrategy::next(const Bar& , size_t currentBarIndex, const std::map<std::string, double>& currentPrices) {
    // Log position status every 500 bars for debugging
    // if (currentBarIndex % 500 == 0 || currentBarIndex < 10) {
    //     Utils::logMessage("Bar " + std::to_string(currentBarIndex) + ": inPosition=" + std::to_string(inPosition) + ", ONE_TRADE=" + std::to_string(one_trade));
    // }

    // --- Exit Logic (For positions) ---
    if (inPosition) {
        if(currentBarIndex == 0)Utils::logMessage(
            "inPosition:" + std::to_string(inPosition)
             + ", taken=" + std::to_string(taken_trade));
        return;
    }
    if (one_trade && taken_trade) {
        return;
    }
    
    // --- Entry Logic ---
    double roll = probDist(rng);
    // Log roll value every 100 bars for debugging
    if (currentBarIndex % 100 == 0) {
        Utils::logMessage("Bar " + std::to_string(currentBarIndex) + ": Roll=" + std::to_string(roll) + ", EntryProbability=" + std::to_string(entryProbability));
    }
    if (roll < entryProbability) {
        // Random direction (BUY or SELL)
        OrderType entryOrderType = (directionDist(rng) == 0) ? OrderType::BUY : OrderType::SELL;
        double desiredSize = benchmarkFixedSize;
        if(entryOrderType == OrderType::SELL) {
            desiredSize = -desiredSize;
        }

        if (debugMode) Utils::logMessage("DEBUG: Random Entry Triggered (Roll: " + std::to_string(roll) + ")");

        // --- Create Order ---
        Order entryOrder;
        entryOrder.type = entryOrderType;
        entryOrder.symbol = dataName;
        entryOrder.reason = OrderReason::ENTRY_SIGNAL;

        entryOrder.requestedSize = desiredSize;
        entryOrder.requestedPrice = currentPrices.at(dataName);

        // For BUY (LONG): TP above entry, SL below entry
        // For SELL (SHORT): TP below entry, SL above entry
        double tpPips = config->getNested<double>("/Strategy/TAKE_PROFIT_PIPS", 30.0);
        double slPips = config->getNested<double>("/Strategy/STOP_LOSS_PIPS", 30.0);

        // Log the raw values for debugging
        Utils::logMessage("DEBUG: TP Pips = " + std::to_string(tpPips) + ", SL Pips = " + std::to_string(slPips));
        Utils::logMessage("DEBUG: Requested Entry Price = " + std::to_string(entryOrder.requestedPrice));

        if (entryOrderType == OrderType::BUY) {
            // Long position
            entryOrder.takeProfit = entryOrder.requestedPrice + tpPips;
            entryOrder.stopLoss = entryOrder.requestedPrice - slPips;

            // Ensure TP is above entry and SL is below entry for long positions
            if (entryOrder.stopLoss >= entryOrder.requestedPrice) {
                Utils::logMessage("WARNING: Correcting invalid SL for long position. SL must be below entry price.");
                entryOrder.stopLoss = entryOrder.requestedPrice - std::abs(slPips);
            }
            if (entryOrder.takeProfit <= entryOrder.requestedPrice) {
                Utils::logMessage("WARNING: Correcting invalid TP for long position. TP must be above entry price.");
                entryOrder.takeProfit = entryOrder.requestedPrice + std::abs(tpPips);
            }
        } else {
            // Short position
            entryOrder.takeProfit = entryOrder.requestedPrice - tpPips;
            entryOrder.stopLoss = entryOrder.requestedPrice + slPips;

            // Ensure TP is below entry and SL is above entry for short positions
            if (entryOrder.stopLoss <= entryOrder.requestedPrice) {
                Utils::logMessage("WARNING: Correcting invalid SL for short position. SL must be above entry price.");
                entryOrder.stopLoss = entryOrder.requestedPrice + std::abs(slPips);
            }
            if (entryOrder.takeProfit >= entryOrder.requestedPrice) {
                Utils::logMessage("WARNING: Correcting invalid TP for short position. TP must be below entry price.");
                entryOrder.takeProfit = entryOrder.requestedPrice - std::abs(tpPips);
            }
        }

        // Log the TP/SL values for debugging
        if (debugMode) {
            Utils::logMessage("DEBUG: Setting " + std::string(entryOrderType == OrderType::BUY ? "LONG" : "SHORT") +
                            " order TP: " + std::to_string(entryOrder.takeProfit) +
                            ", SL: " + std::to_string(entryOrder.stopLoss));
        }

        // entryOrder.takeProfit = config->getNested<double>("/Strategy/TAKE_PROFIT_PIPS", 30.0);
        // entryOrder.stopLoss = config->getNested<double>("/Strategy/STOP_LOSS_PIPS", 30.0);

        currentOrderId = broker->submitOrder(entryOrder);

        if (currentOrderId == -1) {
            Utils::logMessage("ERROR: Failed to submit entry order!");
        } else if (one_trade) {
            taken_trade = true;
        }
        return;
    }

    // Track portfolio value for metrics (every 10 bars to reduce overhead)
    if (currentBarIndex % 10 == 0) {
        double currentValue = broker->getValue(currentPrices);

        // Record return for Sharpe ratio calculation
        double previousValue = metrics->getPreviousValue();
        if (previousValue > 0 && currentBarIndex > 0) {
            double periodReturn = (currentValue - previousValue) / previousValue;
            metrics->recordReturn(periodReturn);

            // Log returns for debugging (every 100 bars)
            if (currentBarIndex % 100 == 0) {
                Utils::logMessage("Bar " + std::to_string(currentBarIndex) + ": Return calculation - Previous: " +
                                std::to_string(previousValue) + ", Current: " + std::to_string(currentValue) +
                                ", Return: " + std::to_string(periodReturn));
            }
        }

        // Update portfolio value AFTER recording return
        metrics->updatePortfolioValue(currentValue);
    }
}

// --- End of Backtest ---
void RandomStrategy::stop() {
    // Get final portfolio value
    double finalValue = broker->getValue({{dataName, data->back().close}});

    // Generate and log the summary report
    std::string report = metrics->generateSummaryReport(finalValue, "RandomStrategy (Theoretical 50/50)");
    Utils::logMessage(report);
}

// --- Order Notification Handler ---
void RandomStrategy::notifyOrder(const Order& order) {
    if (order.id == currentOrderId) {
        currentOrderId = -1;
    }

    if (order.status == OrderStatus::FILLED) {
        // Record commission
        metrics->recordCommission(order.commission);

        // --- Entry Fill ---
        if (order.reason == OrderReason::ENTRY_SIGNAL && !inPosition) {
            Utils::logMessage("Setting inPosition to true after entry");
            inPosition = true;
            currentPosition.symbol = order.symbol;
            currentPosition.size = (order.type == OrderType::BUY) ? order.filledSize : -order.filledSize;
            currentPosition.entryPrice = order.filledPrice;

            std::ostringstream oss;
            oss << ">>> ENTRY EXECUTED <<< | "
                << ((order.type == OrderType::BUY) ? "BUY" : "SELL") << " "
                << order.filledSize << " @ " << order.filledPrice;
            Utils::logMessage(oss.str());

        }
        // --- Exit Fill ---
        else if (inPosition && order.symbol == currentPosition.symbol &&
                (order.reason == OrderReason::TAKE_PROFIT || order.reason == OrderReason::STOP_LOSS ||
                 ((order.type == OrderType::SELL && currentPosition.size > 0.0) ||
                  (order.type == OrderType::BUY && currentPosition.size < 0.0)))) {

            Utils::logMessage("Exit order detected: Reason=" + std::to_string(static_cast<int>(order.reason)) + ", Type=" + std::to_string(static_cast<int>(order.type)) + ", Position Size=" + std::to_string(currentPosition.size));

            // Determine if profitable and record the trade
            bool profitable = (order.reason == OrderReason::TAKE_PROFIT);
            metrics->recordTrade(profitable);

            std::string exitReasonStr = profitable ? "TP" : "SL";

            Utils::logMessage("<<< EXIT EXECUTED (" + exitReasonStr + ") >>> | "
                           + ((order.type == OrderType::BUY) ? "BUY" : "SELL") + " "
                           + std::to_string(order.filledSize) + " @ " + std::to_string(order.filledPrice)
                           + (profitable ? " (PROFIT)" : " (LOSS)"));

            Utils::logMessage("Setting inPosition to false after exit");
            inPosition = false;
            currentPosition = Position();
        }
    }
    else if (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN || order.status == OrderStatus::CANCELLED) {
        Utils::logMessage("--- ORDER ISSUE --- | ID: " + std::to_string(order.id)
                       + " Status: " + std::to_string(static_cast<int>(order.status)));
    }
}
