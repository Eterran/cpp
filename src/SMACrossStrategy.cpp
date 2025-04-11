// SMACrossStrategy.cpp
#include "SMACrossStrategy.h"
#include "Broker.h"     // Include full Broker definition for interaction
#include "Utils.h"      // Logging etc.
#include <cmath>        // std::abs, std::floor
#include <iostream>     // std::cerr for critical errors maybe
#include <iomanip>      // std::setprecision

// --- Constructor ---
// Initialize SMA with a default period. It will be re-initialized in init() if config differs.
SmaCrossStrategy::SmaCrossStrategy() : smaIndicator(200)
{
    // Ensure base class members are initialized via Strategy() constructor
}

std::string SmaCrossStrategy::getName() {
    return name;
}

// --- Initialization ---
void SmaCrossStrategy::init() {
    if (!config || !broker) { // config is now a pointer passed by engine
        throw std::runtime_error("SmaCrossStrategy::init Error: Config pointer or Broker not set.");
    }

    // --- Load Parameters from Config using getNested ---
    try {
        // Use JSON Pointers: "/Section/ParameterName"
        smaPeriod = config->getNested<int>("/Strategy/SMA_PERIOD", 200);
        positionType = config->getNested<std::string>("/Strategy/POSITION_TYPE", "fixed");
        fixedSize = config->getNested<double>("/Strategy/FIXED_SIZE", 100.0);
        cashPercent = config->getNested<double>("/Strategy/CASH_PERCENT", 2.0);

        riskPercent = config->getNested<double>("/Strategy/RISK_PERCENT", 1.0);
        stopLossPips = config->getNested<double>("/Strategy/STOP_LOSS_PIPS", 70.0);
        stopLossEnabled = config->getNested<bool>("/Strategy/STOP_LOSS_ENABLED", true);
        bankruptcyProtection = config->getNested<bool>("/Strategy/BANKRUPTCY_PROTECTION", true);
        forceExitPercent = config->getNested<double>("/Strategy/FORCE_EXIT_PERCENT", -50.0);
        
        takeProfitEnabled = config->getNested<bool>("/Strategy/TAKE_PROFIT_ENABLED", true);
        takeProfitPips = config->getNested<double>("/Strategy/TAKE_PROFIT_PIPS", 100.0);

        debugMode = config->getNested<bool>("/Strategy/DEBUG_MODE", false);

    } catch (const std::exception& e) {
        Utils::logMessage("SmaCrossStrategy::init Warning: Error loading config parameter: " + std::string(e.what()) + ". Using default values.");
        // Ensure defaults are sensible if loading fails
        smaPeriod = 200;
        positionType = "fixed";
        fixedSize = 100;
        //... etc
    }

    // Re-initialize indicator with loaded period if different from default constructor
    if (smaIndicator.getMinPeriod() != static_cast<size_t>(smaPeriod)) {
        smaIndicator = SMAIndicator(smaPeriod); // Replace with correctly configured one
    }

    // --- Set up Strategy State ---
    pipPoint = Utils::calculatePipPoint(dataName); // Calculate pip point for the primary data
    broker->setPointValue(dataName, pipPoint); // Inform broker about pip point

    startingAccountValue = broker->getCash(); // Use initial cash as starting value baseline
    currentPendingOrderId = -1;
    tradeCount = 0;
    profitableTrades = 0;
    inPosition = false;
    bankrupt = false;
    priceAboveSma = false;
    prevPriceAboveSma = false; // Initialize correctly

    // --- Log Initial Setup ---
    Utils::logMessage("--- SmaCrossStrategy Initialized ---");
    Utils::logMessage("Data Name: " + dataName);
    Utils::logMessage("SMA Period: " + std::to_string(smaPeriod));
    Utils::logMessage("Pip Point: " + std::to_string(pipPoint));
    Utils::logMessage("Position Sizing: " + positionType);
    if (positionType == "fixed") Utils::logMessage("  Fixed Size: " + std::to_string(fixedSize));
    else if (positionType == "percent") Utils::logMessage("  Cash Percent: " + std::to_string(cashPercent) + "%");
    else if (positionType == "risk") {
        Utils::logMessage("  Risk Percent: " + std::to_string(riskPercent) + "%");
        Utils::logMessage("  Stop Loss Pips: " + std::to_string(stopLossPips));
    }
    Utils::logMessage("Stop Loss Enabled: " + std::string(stopLossEnabled ? "Yes" : "No"));
    Utils::logMessage("Bankruptcy Protection: " + std::string(bankruptcyProtection ? "Yes" : "No") + " at " + std::to_string(forceExitPercent) + "% Drawdown");
    Utils::logMessage("Debug Mode: " + std::string(debugMode ? "On" : "Off"));
    Utils::logMessage("Starting Account Value (Cash): " + std::to_string(startingAccountValue));
    Utils::logMessage("-------------------------------------");
}

// --- Per-Bar Logic ---
void SmaCrossStrategy::next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) {
    // --- Guard Clauses ---
    if (bankrupt) return; // Stop trading if bankrupt flag is set
    if (currentPendingOrderId != -1) return; // Wait for pending order to resolve

    // Get current account value (needed for checks and sizing)
     // Use the map provided by the engine which should contain currentBar.close for dataName
    double accountValue = broker->getValue(currentPrices);
    double currentPrice = currentBar.close; // Use close price for decisions


    // --- Update Indicator ---
    smaIndicator.update(currentBar);

    // --- Check Indicator Readiness ---
    if (!smaIndicator.hasValue()) {
         // Optional: Log once when SMA becomes valid
         // static bool smaValidLogged = false;
         // if (!smaValidLogged && smaIndicator.getCount() == smaIndicator.getMinPeriod()) {
         //     Utils::logMessage("SMA calculation period reached at bar time: ..."); // Need time formatter
         //     smaValidLogged = true;
         // }
        return;
    }
    double currentSma = smaIndicator.getValue();


    // --- Update Price/SMA State ---
    prevPriceAboveSma = priceAboveSma;
    priceAboveSma = currentPrice > currentSma;

    // --- Calculate Crossovers ---
    bool crossoverAbove = !prevPriceAboveSma && priceAboveSma;
    bool crossoverBelow = prevPriceAboveSma && !priceAboveSma;

    // --- Debug Logging ---
    if (debugMode /* && (crossoverAbove || crossoverBelow || some_time_interval) */) {
        logDebug(currentBar, currentSma, crossoverAbove, crossoverBelow, accountValue);
    }


    // --- Position Management (if in a trade) ---
    if (inPosition) {
        // --- Bankruptcy Protection Check ---
        if (bankruptcyProtection) {
            double drawdownPercent = (accountValue / startingAccountValue - 1.0) * 100.0;
            if (accountValue <= 1.0 || drawdownPercent <= forceExitPercent) {
                Utils::logMessage("!!! BANKRUPTCY PROTECTION TRIGGERED !!!");
                Utils::logMessage("  Account Value: " + std::to_string(accountValue) + " (" + std::to_string(drawdownPercent) + "%)");
                Utils::logMessage("  Threshold: " + std::to_string(forceExitPercent) + "% or Value <= $1.00");
                Utils::logMessage("  Forcing Exit of position " + dataName + " at price " + std::to_string(currentPrice));

                currentPendingOrderId = broker->submitOrder(OrderType::SELL, OrderReason::BANKRUPTCY_PROTECTION, dataName, std::abs(currentPosition.size));
                 if (currentPendingOrderId == -1) { // Handle immediate submission failure
                     Utils::logMessage("  ERROR: Failed to submit bankruptcy exit order!");
                     // What to do here? Stop strategy? Log critical error?
                      bankrupt = true; // Set flag anyway to prevent further actions
                 } else {
                     bankrupt = true; // Set flag to stop further actions after this exit attempt
                 }
                return;
            }
        }

        // --- Take Profit Check ---
        if (takeProfitEnabled && currentPosition.takeProfitPrice > 0 && currentPrice >= currentPosition.takeProfitPrice) {
            double pipsAbove = (currentPrice - currentPosition.takeProfitPrice) / pipPoint;
            Utils::logMessage("--- TAKE PROFIT TRIGGERED ---");
            Utils::logMessage("  Price " + std::to_string(currentPrice) + " >= TP " + std::to_string(currentPosition.takeProfitPrice) + " (" + std::to_string(pipsAbove) + " pips above)");
            Utils::logMessage("  Closing position " + dataName + " due to Take Profit.");

            currentPendingOrderId = broker->submitOrder(OrderType::SELL, OrderReason::TAKE_PROFIT, dataName, std::abs(currentPosition.size));
            if (currentPendingOrderId == -1) {
                Utils::logMessage("  ERROR: Failed to submit take profit exit order!");
            }
            return; 
       }

        // --- Stop Loss Check ---
        if (stopLossEnabled && currentPosition.stopLossPrice > 0) {
            if(debugMode){
                Utils::logMessage("DEBUG SL Check | Current Px (Close): " + std::to_string(currentPrice)
                + " | SL Price: " + std::to_string(currentPosition.stopLossPrice)
                + " | In Position: " + (inPosition?"True":"False"));
            }
            
            if(currentPrice <= currentPosition.stopLossPrice){
                double pipsBelow = std::abs(currentPrice - currentPosition.stopLossPrice) / pipPoint;
                Utils::logMessage("--- STOP LOSS TRIGGERED ---");
                Utils::logMessage("  Price " + std::to_string(currentPrice) + " <= Stop " + std::to_string(currentPosition.stopLossPrice) + " (" + std::to_string(pipsBelow) + " pips below)");
                Utils::logMessage("  Closing position " + dataName + " due to Stop Loss.");

                currentPendingOrderId = broker->submitOrder(OrderType::SELL, OrderReason::STOP_LOSS, dataName, std::abs(currentPosition.size));
                if (currentPendingOrderId == -1) {
                    Utils::logMessage("  ERROR: Failed to submit stop loss exit order!");
                }
                return;
            }
        }

        // --- Exit Signal Check (SMA Crossover Below) ---
        if (crossoverBelow) {
            Utils::logMessage("--- EXIT SIGNAL (Cross Below SMA) ---");
            Utils::logMessage("  Price " + std::to_string(currentPrice) + " crossed BELOW SMA " + std::to_string(currentSma));
            Utils::logMessage("  Closing position " + dataName);

            currentPendingOrderId = broker->submitOrder(OrderType::SELL, OrderReason::EXIT_SIGNAL, dataName, std::abs(currentPosition.size));
            if (currentPendingOrderId == -1) {
                Utils::logMessage("  ERROR: Failed to submit SMA cross exit order!");
            }
            return; // Exit next()
        }

    }
    // --- Entry Logic (if not in a position) ---
    else {
        if (crossoverAbove) {
            Utils::logMessage("--- ENTRY SIGNAL (Cross Above SMA) ---");
            Utils::logMessage("  Price " + std::to_string(currentPrice) + " crossed ABOVE SMA " + std::to_string(currentSma));

            double desiredSize = calculateSize(currentPrice, accountValue);

            if (desiredSize <= 0) {
                Utils::logMessage("  Calculated size is <= 0. No trade placed.");
                return;
            }

            Utils::logMessage("  Attempting BUY order for " + std::to_string(desiredSize) + " units of " + dataName + " at market price approx " + std::to_string(currentPrice));

            // Calculate stop loss price *before* submitting order
            // Note: SL price is stored in the position object *after* the order fills in notifyOrder
            // double potentialStopLoss = currentPrice - (stopLossPips * pipPoint);

            currentPendingOrderId = broker->submitOrder(OrderType::BUY, OrderReason::ENTRY_SIGNAL, dataName, desiredSize);
            if (currentPendingOrderId == -1) {
                Utils::logMessage("  ERROR: Failed to submit entry order!");
            }
            // SL price stored upon fill confirmation in notifyOrder
        }
    }
}

// --- End of Backtest ---
void SmaCrossStrategy::stop() {
    double finalPortfolioValue = broker->getValue({{dataName, data->back().close}}); // Get final value using last price

    Utils::logMessage("--- SmaCrossStrategy Finished ---");
    if (bankrupt) {
         Utils::logMessage("Trading stopped early due to Bankruptcy Protection trigger.");
    }
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
    // Optional: Dump trade history from broker->getOrderHistory() here
}

// --- Order Notification Handler ---
void SmaCrossStrategy::notifyOrder(const Order& order) {
    // Check if this notification pertains to the order we were waiting for
    if (order.id == currentPendingOrderId) {
        currentPendingOrderId = -1; // Reset pending order flag regardless of outcome
    }

    // Log details based on order status
    std::stringstream ss;
    ss << std::fixed << std::setprecision(5); // Set precision for price logging
    ss << "Notify Order: ID " << order.id << " | " << dataName << " | Type " << (order.type == OrderType::BUY ? "BUY" : "SELL");
    ss << " | Status " << static_cast<int>(order.status); // TODO: Convert status/reason enums to string
    ss << " | Reason " << static_cast<int>(order.reason);
    ss << " | Req Size " << order.requestedSize;


    if (order.status == OrderStatus::FILLED) {
        ss << " | Filled Size " << order.filledSize << " @ Price " << order.filledPrice;
        ss << " | Commission " << std::setprecision(2) << order.commission;

        if (order.type == OrderType::BUY && order.reason == OrderReason::ENTRY_SIGNAL) {
            // --- Entry Fill ---
             if(inPosition) {
                 // This shouldn't happen with current logic (only enter if not inPosition)
                  Utils::logMessage("Notify Warning: Received BUY entry fill notification while already in position!");
             } else {
                tradeCount++;
                inPosition = true;
                // Store position details
                currentPosition.symbol = order.symbol;
                currentPosition.size = order.filledSize; // BUY is positive size
                currentPosition.entryPrice = order.filledPrice;
                currentPosition.entryTime = order.executionTime;
                currentPosition.pipPoint = pipPoint; // Store pip point
                currentPosition.lastValue = std::abs(currentPosition.size * currentPosition.entryPrice);
                currentPosition.takeProfitPrice = order.filledPrice + (takeProfitPips * pipPoint);
                if(debugMode)
                    Utils::logMessage("BUY FILL DEBUG | ID: " + std::to_string(order.id) + " | Stored Entry Px (Ask?): " + std::to_string(currentPosition.entryPrice));

                // Calculate and store stop loss price for this position
                if (stopLossEnabled) {
                    currentPosition.stopLossPrice = order.filledPrice - (stopLossPips * pipPoint);
                    ss << " | SL Set: " << std::fixed << std::setprecision(5) << currentPosition.stopLossPrice;
                } else {
                    currentPosition.stopLossPrice = 0.0; // No stop loss
                }
                Utils::logMessage(">>> BUY EXECUTED <<< | Trade #" + std::to_string(tradeCount) + " | Details: " + ss.str());
            }

        } else if (order.type == OrderType::SELL && inPosition) {
             // --- Exit Fill ---
             // Ensure this sell order corresponds to closing the current position
            if (order.symbol == currentPosition.symbol && order.filledSize >= std::abs(currentPosition.size)) { // Allow for exact or slightly larger fill size to close
                double pnl = (order.filledPrice - currentPosition.entryPrice) * currentPosition.size;
                double pips = (order.filledPrice - currentPosition.entryPrice) / pipPoint * (currentPosition.size > 0 ? 1 : -1) ; // Pips calculation adjusted for direction
                bool profitable = pnl > 0;

                if(debugMode){
                    std::stringstream dbg_ss;
                    dbg_ss << std::fixed << std::setprecision(5);
                    dbg_ss << "SELL FILL DEBUG | ID: " << order.id
                        << " | Reason: " << static_cast<int>(order.reason)
                        << " | Entry Px (Ask?): " << currentPosition.entryPrice
                        << " | Exit Px (Bid?): " << order.filledPrice
                        << " | Size: " << currentPosition.size
                        << " | PnL Calc: " << pnl;
                    Utils::logMessage(dbg_ss.str());
                }

                if (profitable) {
                    profitableTrades++;
                }
                ss << " | P/L: " << std::fixed << std::setprecision(2) << pnl;
                ss << " (" << std::fixed << std::setprecision(1) << pips << " pips)";
                ss << " | Result: " << (profitable ? "PROFIT" : "LOSS");

                Utils::logMessage("<<< SELL EXECUTED >>> | Closing Trade #" + std::to_string(tradeCount) + " | Details: " + ss.str());

                // Reset position state
                inPosition = false;
                currentPosition = Position(); // Reset to default state

            } else {
                // Sell order filled, but not matching the expected close of currentPosition
                Utils::logMessage("Notify Warning: Received SELL fill notification that doesn't match open position close criteria. Order: " + ss.str());
            }

        } else {
             // Order filled, but not a standard entry/exit according to current state? (e.g., partial close fill?)
             Utils::logMessage("Notify Info: Received uncategorized FILL notification: " + ss.str());
        }


    } else if (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN || order.status == OrderStatus::CANCELLED) {
        ss << " | Status: " << static_cast<int>(order.status); // Add status string conversion
        Utils::logMessage("--- ORDER ISSUE --- | Details: " + ss.str());
        // If an entry order was rejected, we need to ensure inPosition is false.
        if (order.reason == OrderReason::ENTRY_SIGNAL && (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN)) {
            // No state change needed as entry didn't happen. Logged mainly for info.
        }
        // If an exit order was rejected, the position remains open! Critical issue.
        if ((order.reason == OrderReason::EXIT_SIGNAL || order.reason == OrderReason::STOP_LOSS || order.reason == OrderReason::BANKRUPTCY_PROTECTION) &&
            (order.status == OrderStatus::REJECTED || order.status == OrderStatus::MARGIN))
        {
            Utils::logMessage("CRITICAL WARNING: Exit Order " + std::to_string(order.id) + " was REJECTED/MARGIN! Position remains open!");
            // Strategy might need logic here to retry exit? Or stop trading?
            // Setting bankrupt=true might be safest if bankruptcy exit fails.
            if (order.reason == OrderReason::BANKRUPTCY_PROTECTION) bankrupt = true;
        }

    } else {
        // Other statuses like SUBMITTED, ACCEPTED - usually just informational
        // Utils::logMessage("Notify Info: Order Update - " + ss.str());
    }
}


// --- Helper Methods ---
double SmaCrossStrategy::calculateSize(double currentPrice, double accountValue) {
    if (currentPrice <= 0) {
        Utils::logMessage("SmaCrossStrategy::calculateSize Warning: Invalid currentPrice (" + std::to_string(currentPrice) + ")");
        return 0;
    }

    double units = 0;

    if (positionType == "fixed") {
        units = fixedSize;
        Utils::logMessage("  Sizing: Fixed - " + std::to_string(units) + " units");

    } else if (positionType == "percent") {
        double cash = broker->getCash();
        double sizeInValue = cash * (cashPercent / 100.0);
        units = sizeInValue / currentPrice; // Ignores margin/leverage for simplicity (like Python example)
        Utils::logMessage("  Sizing: Percent - " + std::to_string(cashPercent) + "% of cash (" + std::to_string(cash) + ") -> Value " + std::to_string(sizeInValue) + " -> Units " + std::to_string(units));

    } else if (positionType == "risk") {
        if (stopLossPips <= 0) {
            Utils::logMessage("  Sizing Error: Stop loss pips must be positive for risk calculation.");
            return 0;
        }

        double riskAmount = accountValue * (riskPercent / 100.0);
        // double slDistanceValue = stopLossPips * pipPoint; // Stop distance in price terms

        // Pip value calculation: Value of 1 pip move for 1 unit of the asset in account currency (assume USD)
        double pipValuePerUnit = 0.0;
        if (dataName.find("USDJPY") != std::string::npos) { // Specific to USD/JPY
             pipValuePerUnit = pipPoint / currentPrice;
        } else if (dataName.rfind("USD") == dataName.length() - 3) { // Ends with USD (e.g., EURUSD)
             pipValuePerUnit = pipPoint; // For 1 unit, pip value is fixed in USD
        } else {
             Utils::logMessage("  Sizing Error: Pip value calculation not implemented for pair '" + dataName + "' in risk sizing.");
             return 0;
        }

        if (pipValuePerUnit <= 0) {
            Utils::logMessage("  Sizing Error: Calculated invalid pip value per unit (" + std::to_string(pipValuePerUnit) + ")");
            return 0;
        }

        double riskPerUnit = stopLossPips * pipValuePerUnit;
        // This is equivalent to: double riskPerUnit = slDistanceValue * (pipValuePerUnit / pipPoint); ??? No, simpler is stop_pips * value_per_pip

        if (riskPerUnit <= 0) {
             Utils::logMessage("  Sizing Error: Calculated invalid risk per unit (" + std::to_string(riskPerUnit) + ")");
             return 0;
        }

        units = riskAmount / riskPerUnit;

        Utils::logMessage("  Sizing: Risk - " + std::to_string(riskPercent) + "% of Account (" + std::to_string(accountValue) + ") -> Risk Amount " + std::to_string(riskAmount));
        Utils::logMessage("    SL Pips: " + std::to_string(stopLossPips) + " | Pip Value/Unit: " + std::to_string(pipValuePerUnit) + " | Risk/Unit: " + std::to_string(riskPerUnit));
        Utils::logMessage("    Calculated Units: " + std::to_string(units));

    } else {
        Utils::logMessage("  Sizing Error: Unknown position_type: " + positionType);
        return 0;
    }

    // Apply rounding and minimums (e.g., floor to be conservative, min 1 unit)
    // Brokers often have lot sizes (e.g., multiples of 1000). Add later if needed.
    double finalUnits = std::max(1.0, std::floor(units)); // Ensure at least 1 unit, floor for risk modes
    if (positionType == "percent" || positionType == "fixed") {
         // Maybe round instead of floor for fixed/percent? Let's stick to floor for consistency.
    }

    Utils::logMessage("  Final Units: " + std::to_string(finalUnits));
    return finalUnits;
}


void SmaCrossStrategy::logDebug(const Bar& bar, double currentSma, bool crossoverAbove, bool crossoverBelow, double accountValue) {
    // Basic debug log - could add time interval checks later
    std::stringstream ss;
    // Add timestamp formatting if available in Utils
    ss << "DEBUG | " << Utils::formatTimestamp(bar.timestamp); // Use formatter
    ss << " | Close:" << std::fixed << std::setprecision(5) << bar.close;
    ss << " | SMA:" << std::fixed << std::setprecision(5) << currentSma;
    ss << " | P>SMA:" << (priceAboveSma ? "T" : "F");
    ss << " | XUp:" << (crossoverAbove ? "T" : "F");
    ss << " | XDn:" << (crossoverBelow ? "T" : "F");
    ss << " | Pos:" << (inPosition ? currentPosition.size : 0);
    ss << " | PendID:" << currentPendingOrderId;
    ss << " | AccVal:" << std::fixed << std::setprecision(2) << accountValue;
    Utils::logMessage(ss.str());
}