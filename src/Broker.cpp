// Broker.cpp
#include "Broker.h"
#include "Strategy.h"
#include "Utils.h"    // For logging and pip point calculation
#include <cmath>      // For std::abs, std::round etc.
#include <stdexcept>  // For potential errors (though mostly handled via logging/rejection)
#include <numeric>    // For std::accumulate
#include <chrono>     // For random seed
#include <sstream>    // For random number generation (slippage)

// --- Constructor ---
Broker::Broker(double initialCash, double lev, double commRate) :
    startingCash(initialCash),
    cash(initialCash),
    leverage(lev > 0 ? lev : 1.0),
    commissionRate(commRate),
    nextOrderId(1),
    strategy(nullptr)
    // rng(static_cast<unsigned int>(
    //     std::chrono::system_clock::now().time_since_epoch().count())
    // )
    // slippageDist(0.000, 0.000) // DISABLE slippage
{
    if (initialCash <= 0) {
        Utils::logMessage("Broker Warning: Initial cash is zero or negative."); // PROBABLY throw error
        initialCash = 1.0;
    }
    Utils::logMessage("Broker initialized. Start Cash: " + std::to_string(initialCash)
                        + ", Leverage: " + std::to_string(leverage)
                        + ", Commission/Unit: " + std::to_string(commissionRate));
}

// Default constructor implementation
Broker::Broker() :
    startingCash(10000.0),
    cash(10000.0),
    leverage(1.0),
    commissionRate(0.0),
    nextOrderId(1),
    strategy(nullptr)
    // rng(static_cast<unsigned int>(
    //     std::chrono::system_clock::now().time_since_epoch().count())
    // )
    // slippageDist(-0.002, 0.002) // ±0.2% slippage
{
    Utils::logMessage("Broker initialized with default values. Start Cash: 10000, Leverage: 1.0, Commission: 0.0");
}

// --- Configuration ---
void Broker::setStrategy(Strategy* strat) {
    strategy = strat;
}

// --- Helpers ---
double Broker::getFillPrice(const Bar& bar, OrderType orderType, double requestedPrice) const {
    // If a specific price is requested, use that price
    if (requestedPrice > 0) {
        return requestedPrice;
    }

    // Otherwise use market prices
    if (orderType == OrderType::BUY) {
        // Buyer gets the ASK price (higher)
        return (bar.ask > 0) ? bar.ask : bar.close; // Use ask if available, else close
    } else { // SELL
        // Seller gets the BID price (lower)
        return (bar.bid > 0) ? bar.bid : bar.close; // Use bid if available, else close
    }
}

double Broker::calculateMarginNeeded(double size, double price) const {
    if (leverage <= 0) return std::abs(size * price); // No leverage, full amount needed
    return std::abs(size * price) / leverage;
}

double Broker::calculateCommission(double size, double price) const {
    // Calculate commission as a percentage of the trade value
    // commissionRate is in percentage (e.g., 0.06 means 0.06%)
    double tradeValue = std::abs(size) * price;
    double commission = tradeValue * (commissionRate / 100.0);

    // Debug log to verify calculation
    Utils::logMessage("Commission calculation: Size=" + std::to_string(std::abs(size)) +
                     ", Price=" + std::to_string(price) +
                     ", Rate=" + std::to_string(commissionRate) +
                     "%, Result=" + std::to_string(commission));

    return commission;
}

double Broker::getPointValue(const std::string&) const {
    // TODO: actually calculate point value based on symbol
    return 1.0;
}

// --- Account Info ---
double Broker::getStartingCash() const {
    return startingCash;
}

double Broker::getCash() const {
    return cash;
}

// calculate acc value including floating PnL
double Broker::getValue(const std::map<std::string, double>& currentPrices) {
    double totalValue = cash;
    double positionsValue = 0.0;

    for (const auto& pair : positions) {
        const std::string& symbol = pair.first;
        const Position& pos = pair.second;

        auto priceIt = currentPrices.find(symbol);
        if (priceIt != currentPrices.end()) {
            // For short positions (negative size), PnL should be negative when current price > entry price
            positionsValue += pos.calculateUnrealizedPnL(priceIt->second);

            // Debug logging for significant positions to help troubleshoot
            if (std::abs(pos.size) > 0.01) {
                Utils::logMessage("Position PnL calculation: Symbol=" + symbol +
                                 ", Size=" + std::to_string(pos.size) +
                                 ", Entry=" + std::to_string(pos.entryPrice) +
                                 ", Current=" + std::to_string(priceIt->second) +
                                 ", PnL=" + std::to_string(pos.calculateUnrealizedPnL(priceIt->second)));
            }
        } else {
            // If price for an open position isn't available, we can't calculate its current value.
            // Option 1: Log warning and exclude it (underestimates value).
            // Option 2: Use last known price (might be stale).
            // Option 3: Use entry price (assumes no change).
            Utils::logMessage("Broker::getValue Warning: Current price for open position '" + symbol + "' not provided. Using entry value for PnL calculation (PnL = 0 for this asset).");
            // Effectively adds 0 PnL for this asset in this case
             // positionsValue += 0.0;
              // Or try adding the original cost back? No, stick to PnL calculation method.
              // Add the nominal entry value maybe? Needs careful thought. Let's stick to PnL calculation.
              // Unrealized PnL calculation inherently handles this - if price isn't found, it cant calculate.
              // Recalculate how getValue should work: Value = Cash + Sum(CurrentValue of Assets)
              // CurrentValue = EntryCost + UnrealizedPnL
              // Let's recalculate simpler: Value = Cash + Sum(Size * CurrentPrice)

             // Alternative getValue: Sum of liquidation value
             // positionsValue += pos.size * priceIt->second; // This calculates the current nominal value of the position asset
        }
    }
    // Correct approach: Account Value = Cash + Unrealized PnL
    totalValue += positionsValue;
    return totalValue;

    /* --- Alternative getValue Calculation (Liquidation Value) ---
    double liquidationValue = cash;
    for (const auto& pair : positions) {
        const std::string& symbol = pair.first;
        const Position& pos = pair.second;
        auto priceIt = currentPrices.find(symbol);
        if (priceIt != currentPrices.end()) {
            liquidationValue += pos.size * priceIt->second; // Add market value of holding
        } else {
             Utils::logMessage("Broker::getValue Warning: Current price for open position '" + symbol + "' not provided. Cannot include its market value in total.");
        }
    }
    return liquidationValue;
    */
}

// --- Helper: Reject Order ---
void Broker::rejectOrder(Order& order, OrderStatus rejectionStatus, const Bar& executionBar) {
    order.status = rejectionStatus;
    order.executionTime = executionBar.timestamp;
    orderHistory.push_back(order); // Move to history
    if (strategy) {
        strategy->notifyOrder(order);
    }
    Utils::logMessage("Broker: Order " + std::to_string(order.id) + " REJECTED (" + std::to_string(static_cast<int>(rejectionStatus)) + ")"); // Log rejection reason
}

// --- Helper: Execute Open Order ---
// Handles opening a new position or increasing an existing one
void Broker::executeOpenOrder(Order& order, const Bar& executionBar) {
    double fillPrice = getFillPrice(executionBar, order.type, order.requestedPrice);
    if (fillPrice <= 0) {
        Utils::logMessage("Broker Warning: Invalid fill price for open order " + std::to_string(order.id));
        rejectOrder(order, OrderStatus::REJECTED, executionBar);
        return;
    }

    double marginNeeded = calculateMarginNeeded(order.requestedSize, fillPrice);
    double commission = calculateCommission(order.requestedSize, fillPrice);

    // --- Perform Checks ---
    bool checksPassed = true;
    OrderStatus rejectionStatus = OrderStatus::REJECTED;
    if (marginNeeded > cash) {
        Utils::logMessage("Broker: Open Order " + std::to_string(order.id) + " REJECTED (Margin). Needed: " + std::to_string(marginNeeded) + ", Cash: " + std::to_string(cash));
        rejectionStatus = OrderStatus::MARGIN;
        checksPassed = false;
    } else if (commission > cash - marginNeeded) {
        Utils::logMessage("Broker: Open Order " + std::to_string(order.id) + " REJECTED (Cash for Commission). Comm: " + std::to_string(commission) + ", Cash after Margin: " + std::to_string(cash - marginNeeded));
        rejectionStatus = OrderStatus::REJECTED;
        checksPassed = false;
    }

    if (!checksPassed) {
        rejectOrder(order, rejectionStatus, executionBar);
        return;
    }

    // --- Checks Passed - Apply Execution ---
    order.status = OrderStatus::FILLED;
    order.filledPrice = fillPrice;
    order.filledSize = order.requestedSize; // Assume full fill for open orders
    order.commission = commission;
    order.executionTime = executionBar.timestamp;

    cash -= order.commission; // Deduct commission

    // Find if position exists to increase it
    Position* existingPosition = nullptr;
    auto posIt = positions.find(order.symbol);
    if (posIt != positions.end()) {
        existingPosition = &(posIt->second);
    }

    if (existingPosition) { // Increase existing position
        Utils::logMessage("Broker: Increasing position " + order.symbol + ". Added Size: " + std::to_string(order.filledSize));
        double currentSize = existingPosition->size;
        double currentEntry = existingPosition->entryPrice;
        double newSize = currentSize + order.filledSize;
        // Calculate new average entry price
        existingPosition->entryPrice = ((currentSize * currentEntry) + (order.filledSize * fillPrice)) / newSize;
        existingPosition->size = newSize;
        existingPosition->lastValue = std::abs(newSize * existingPosition->entryPrice);
        // SL/TP might need recalculation/management by strategy after notification
         Utils::logMessage("Broker: New position size " + std::to_string(newSize) + ", Avg Entry: " + std::to_string(existingPosition->entryPrice));

    } else { // Create new position
        Position newPos;
        newPos.symbol = order.symbol;
        newPos.size = order.filledSize;
        newPos.entryPrice = fillPrice;
        newPos.entryTime = order.executionTime;
        newPos.pointValue = getPointValue(order.symbol);
        newPos.lastValue = std::abs(newPos.size * newPos.entryPrice);
        // Set SL/TP based *on the order* if provided, otherwise strategy manages
        // Validate SL/TP values before setting them
        if (newPos.size > 0) { // Long position
            // For long positions, SL should be below entry, TP above entry
            if (order.stopLoss > 0 && order.stopLoss < newPos.entryPrice) {
                newPos.stopLoss = order.stopLoss;
            } else if (order.stopLoss > 0) {
                Utils::logMessage("Broker Warning: Invalid Stop Loss for long position. SL must be below entry price.");
                // Set a valid SL below entry price
                newPos.stopLoss = newPos.entryPrice * 0.99; // 1% below entry price as a fallback
            }

            if (order.takeProfit > 0 && order.takeProfit > newPos.entryPrice) {
                newPos.takeProfit = order.takeProfit;
            } else if (order.takeProfit > 0) {
                Utils::logMessage("Broker Warning: Invalid Take Profit for long position. TP must be above entry price.");
                // Set a valid TP above entry price
                newPos.takeProfit = newPos.entryPrice * 1.01; // 1% above entry price as a fallback
            }
        } else { // Short position
            // For short positions, SL should be above entry, TP below entry
            if (order.stopLoss > 0 && order.stopLoss > newPos.entryPrice) {
                newPos.stopLoss = order.stopLoss;
            } else if (order.stopLoss > 0) {
                Utils::logMessage("Broker Warning: Invalid Stop Loss for short position. SL must be above entry price.");
                // Set a valid SL above entry price
                newPos.stopLoss = newPos.entryPrice * 1.01; // 1% above entry price as a fallback
            }

            if (order.takeProfit > 0 && order.takeProfit < newPos.entryPrice) {
                newPos.takeProfit = order.takeProfit;
            } else if (order.takeProfit > 0) {
                Utils::logMessage("Broker Warning: Invalid Take Profit for short position. TP must be below entry price.");
                // Set a valid TP below entry price
                newPos.takeProfit = newPos.entryPrice * 0.99; // 1% below entry price as a fallback
            }
        }
        positions[order.symbol] = newPos;

        std::string direction = (newPos.size > 0) ? "LONG" : "SHORT";
        Utils::logMessage("Broker: Opening " + direction + " position " + order.symbol +
                         ". Size: " + std::to_string(std::abs(newPos.size)) +
                         ", Entry: " + std::to_string(newPos.entryPrice) +
                         ", SL: " + std::to_string(newPos.stopLoss) +
                         ", TP: " + std::to_string(newPos.takeProfit) +
                         ", Commission: " + std::to_string(order.commission));
    }

    // --- Finalize ---
    orderHistory.push_back(order);
    if (strategy) {
        strategy->notifyOrder(order);
    }
}

// --- Helper: Execute Close Order ---
// Handles closing, reducing, or reversing a position
void Broker::executeCloseOrder(Order& order, Position& existingPosition, const Bar& executionBar) {
     double fillPrice = getFillPrice(executionBar, order.type, order.requestedPrice);
    if (fillPrice <= 0) {
        Utils::logMessage("Broker Warning: Invalid fill price for close order " + std::to_string(order.id));
        rejectOrder(order, OrderStatus::REJECTED, executionBar);
        return;
    }

    // Determine actual size to close (cannot close more than exists)
    double sizeToClose = std::min(order.requestedSize, std::abs(existingPosition.size));
    if (sizeToClose < 1e-9) { // Avoid closing zero or tiny amounts
         Utils::logMessage("Broker Info: Close order " + std::to_string(order.id) + " has negligible size (" + std::to_string(sizeToClose) + "). Rejecting.");
         rejectOrder(order, OrderStatus::REJECTED, executionBar);
         return;
    }

    double commission = calculateCommission(sizeToClose, fillPrice);

    // --- Perform Checks ---
    bool checksPassed = true;
    OrderStatus rejectionStatus = OrderStatus::REJECTED;
    if (commission > cash) { // Simple check for closing orders
        Utils::logMessage("Broker: Close Order " + std::to_string(order.id) + " REJECTED (Cash for Commission). Comm: " + std::to_string(commission) + ", Cash: " + std::to_string(cash));
        checksPassed = false;
    }

    if (!checksPassed) {
        rejectOrder(order, rejectionStatus, executionBar);
        return;
    }

    // --- Checks Passed - Apply Execution ---
    order.status = OrderStatus::FILLED;
    order.filledPrice = fillPrice;
    order.filledSize = sizeToClose; // Fill the actual size being closed
    order.commission = commission;
    order.executionTime = executionBar.timestamp;

    cash -= order.commission; // Deduct commission

    // --- Calculate PnL on the closed portion ---
    // Sign of closed portion is opposite of existingPosition.size
    double closedSizeSigned = (existingPosition.size > 0) ? -sizeToClose : sizeToClose;

    // Calculate PnL differently for long vs short positions
    double pnl = 0.0;
    if (existingPosition.size > 0) { // Long position
        // For long: (exit price - entry price) * size
        pnl = (fillPrice - existingPosition.entryPrice) * closedSizeSigned;
    } else { // Short position
        // For short: (entry price - exit price) * size
        pnl = (existingPosition.entryPrice - fillPrice) * closedSizeSigned;
    }

    cash += pnl; // Add realized P/L to cash

    // --- Update Position ---
    double newPositionSize = existingPosition.size + closedSizeSigned; // Add signed closed amount

    if (std::abs(newPositionSize) < 1e-9) { // Position fully closed
        // Create a more detailed log message with clear PnL calculation
        std::string direction = (existingPosition.size > 0) ? "LONG" : "SHORT";
        std::string pnlCalcStr = (existingPosition.size > 0) ?
            "(Exit " + std::to_string(fillPrice) + " - Entry " + std::to_string(existingPosition.entryPrice) + ") * Size " + std::to_string(closedSizeSigned) :
            "(Entry " + std::to_string(existingPosition.entryPrice) + " - Exit " + std::to_string(fillPrice) + ") * Size " + std::to_string(closedSizeSigned);

        Utils::logMessage("Broker: Closing " + direction + " position " + order.symbol + ". Closed Size: " + std::to_string(std::abs(closedSizeSigned))
                            + ", Entry: " + std::to_string(existingPosition.entryPrice)
                            + ", Exit: " + std::to_string(fillPrice)
                            + ", PnL: " + std::to_string(pnl) + " [" + pnlCalcStr + "]"
                            + ", Commission: " + std::to_string(order.commission));
        positions.erase(order.symbol); // Remove from map
    } else { // Position reduced (partial close)
        std::string direction = (existingPosition.size > 0) ? "LONG" : "SHORT";
        std::string pnlCalcStr = (existingPosition.size > 0) ?
            "(Exit " + std::to_string(fillPrice) + " - Entry " + std::to_string(existingPosition.entryPrice) + ") * Size " + std::to_string(closedSizeSigned) :
            "(Entry " + std::to_string(existingPosition.entryPrice) + " - Exit " + std::to_string(fillPrice) + ") * Size " + std::to_string(closedSizeSigned);

        Utils::logMessage("Broker: Reducing " + direction + " position " + order.symbol + ". Closed Size: " + std::to_string(std::abs(closedSizeSigned))
                            + ", Entry: " + std::to_string(existingPosition.entryPrice)
                            + ", Exit: " + std::to_string(fillPrice)
                            + ", PnL: " + std::to_string(pnl) + " [" + pnlCalcStr + "]");
        existingPosition.size = newPositionSize; // Update remaining size
        // Entry price, SL/TP remain the same for the remaining portion
        Utils::logMessage("Broker: Remaining position " + order.symbol + ". Size: " + std::to_string(newPositionSize));
    }

    // --- Finalize ---
    orderHistory.push_back(order);
    if (strategy) {
        strategy->notifyOrder(order);
    }
}

// Apply random slippage to price
// double Broker::applySlippage(double basePrice, bool isFavorable) {
//     // Generate random slippage factor
//     double slippageFactor = slippageDist(rng);

//     // If not favorable, ensure slippage is only negative (worse for trader)
//     if (!isFavorable && slippageFactor > 0) {
//         slippageFactor = -slippageFactor;
//     }
//     // If favorable, ensure slippage is only positive (better for trader)
//     else if (isFavorable && slippageFactor < 0) {
//         slippageFactor = -slippageFactor;
//     }

//     // Apply slippage to base price
//     double slippagePrice = basePrice * (1.0 + slippageFactor);

//     // Log the slippage calculation for debugging
//     Utils::logMessage("Broker: Slippage calculation - Base price: " + std::to_string(basePrice) +
//                      ", Factor: " + std::to_string(slippageFactor) +
//                      ", Result: " + std::to_string(slippagePrice) +
//                      ", Favorable: " + (isFavorable ? "true" : "false"));

//     return slippagePrice;
// }

// Check if positions hit take profit or stop loss levels
void Broker::checkTakeProfitStopLoss(const Bar& currentBar, const std::map<std::string, double>& currentPrices) {
    try {
        // Iterate through all open positions
        for (auto posIt = positions.begin(); posIt != positions.end(); /* no increment */) {
            const std::string& symbol = posIt->first;
            Position& position = posIt->second;

            // Skip positions with no TP/SL set
            if (position.takeProfit <= 0.0 && position.stopLoss <= 0.0) {
                ++posIt;
                continue;
            }

            // Get current price
            auto priceIt = currentPrices.find(symbol);
            if (priceIt == currentPrices.end()) {
                Utils::logMessage("Broker::checkTakeProfitStopLoss Warning: No price found for symbol '" + symbol + "'");
                ++posIt;
                continue;
            }

            double currentPrice = priceIt->second;
            bool tpHit = false, slHit = false;
            OrderReason closeReason = OrderReason::EXIT_SIGNAL; // Default

        // Check if TP/SL hit
        if (position.size > 0) { // Long position
            if (position.takeProfit > 0 && currentPrice >= position.takeProfit) {
                tpHit = true;
                closeReason = OrderReason::TAKE_PROFIT;
                Utils::logMessage("Broker: Take Profit hit for LONG position in " + symbol +
                                 ". Current price: " + std::to_string(currentPrice) +
                                 ", TP level: " + std::to_string(position.takeProfit));
            } else if (position.stopLoss > 0 && position.stopLoss < position.entryPrice && currentPrice <= position.stopLoss) {
                slHit = true;
                closeReason = OrderReason::STOP_LOSS;
            } else if (position.stopLoss > 0 && position.stopLoss >= position.entryPrice) {
                // Log warning for incorrectly set stop loss
                Utils::logMessage("Broker Warning: Stop loss (" + std::to_string(position.stopLoss) +
                                 ") for long position in " + symbol +
                                 " is above entry price (" + std::to_string(position.entryPrice) +
                                 "). Stop loss should be below entry price for long positions.");
                // Invalidate the stop loss
                position.stopLoss = 0.0;
            }
        } else if (position.size < 0) { // Short position
            if (position.takeProfit > 0 && currentPrice <= position.takeProfit) {
                tpHit = true;
                closeReason = OrderReason::TAKE_PROFIT;
                Utils::logMessage("Broker: Take Profit hit for SHORT position in " + symbol +
                                 ". Current price: " + std::to_string(currentPrice) +
                                 ", TP level: " + std::to_string(position.takeProfit));
            } else if (position.stopLoss > 0 && position.stopLoss > position.entryPrice && currentPrice >= position.stopLoss) {
                slHit = true;
                closeReason = OrderReason::STOP_LOSS;
            } else if (position.stopLoss > 0 && position.stopLoss <= position.entryPrice) {
                // Log warning for incorrectly set stop loss
                Utils::logMessage("Broker Warning: Stop loss (" + std::to_string(position.stopLoss) +
                                 ") for short position in " + symbol +
                                 " is below entry price (" + std::to_string(position.entryPrice) +
                                 "). Stop loss should be above entry price for short positions.");
                // Invalidate the stop loss
                position.stopLoss = 0.0;
            }
        }

        // If TP/SL hit, create and execute close order
        if (tpHit || slHit) {
            OrderType closeType = (position.size > 0) ? OrderType::SELL : OrderType::BUY;

            // Create a market order to close the position
            Order closeOrder;
            closeOrder.id = nextOrderId++;
            closeOrder.type = closeType;
            closeOrder.symbol = symbol;
            closeOrder.requestedSize = std::abs(position.size);
            closeOrder.status = OrderStatus::SUBMITTED;
            closeOrder.reason = closeReason;
            closeOrder.creationTime = std::chrono::system_clock::now();

            // Apply slippage to the price (favorable for TP, unfavorable for SL)
            double targetPrice = tpHit ? position.takeProfit : position.stopLoss;

            // For long positions with TP, slippage should be favorable (higher price)
            // For short positions with TP, slippage should be favorable (lower price)
            // For SL, slippage is always unfavorable
            // bool isFavorable = tpHit;

            // DONT ENABLE SLIPPAGE
            // double slippagePrice = applySlippage(targetPrice, isFavorable);
            // closeOrder.requestedPrice = slippagePrice;
            closeOrder.requestedPrice = targetPrice;

            // Log the slippage calculation for debugging
            // std::string orderTypeStr = tpHit ? "TAKE PROFIT" : "STOP LOSS";
            // Utils::logMessage("Broker: Applied slippage to " + orderTypeStr +
                            //  " price. Original: " + std::to_string(targetPrice) +
                            //  ", After slippage: " + std::to_string(closeOrder.requestedPrice));

            std::ostringstream oss;
            oss << "Broker: Auto-executing "
                << (tpHit ? "TAKE PROFIT" : "STOP LOSS")
                << " order for " << symbol
                << " at price " << closeOrder.requestedPrice
                << " (original target: " << targetPrice << ")";
            Utils::logMessage(oss.str());

            // Execute the order
            executeCloseOrder(closeOrder, position, currentBar);
            // Note: executeCloseOrder already adds the order to orderHistory and notifies the strategy
            // So we don't need to do it again here

            // If position was fully closed, the iterator is invalid
            // Note: executeCloseOrder already removes the position from the map if it's fully closed
            // So we don't need to check position.size here, just break out of the loop
            // since we've modified the positions map and the iterator might be invalid
            break; // Exit the loop after closing a position
        } else {
            ++posIt;
        }
    }
    } catch (const std::exception& e) {
        Utils::logMessage("Broker::checkTakeProfitStopLoss Error: Exception caught: " + std::string(e.what()));
    } catch (...) {
        Utils::logMessage("Broker::checkTakeProfitStopLoss Error: Unknown exception caught");
    }
}

// --- Order Management ---
// returns order ID
int Broker::submitOrder(Order order) { // Pass Order struct by value (makes a copy)
    if (strategy == nullptr) {
        Utils::logMessage("Broker Error: Strategy not set. Cannot submit order.");
        return -1;
    }

    // Assign unique ID and timestamp before adding
    order.id = nextOrderId++;
    order.status = OrderStatus::SUBMITTED; // Mark as ready for processing
    order.creationTime = std::chrono::system_clock::now();

    pendingOrders.push_back(order); // Add the modified copy to pending queue
    Utils::logMessage("Broker: Order " + std::to_string(order.id) + " submitted. Type: " + (order.type == OrderType::BUY ? "BUY" : "SELL") + ", Size: " + std::to_string(order.requestedSize) + ", Symbol: " + order.symbol);

    return order.id; // Return the assigned ID
}

// --- Process Orders Loop (Refactored) ---
void Broker::processOrders(const Bar& currentBar) {
    if (strategy == nullptr) return;

    try {
        // First check if any positions hit take profit or stop loss
        std::map<std::string, double> currentPrices;
        // Create a map with close prices for all symbols with open positions
        for (const auto& pair : positions) {
            currentPrices[pair.first] = currentBar.close;
        }
        checkTakeProfitStopLoss(currentBar, currentPrices);

        // Process pending orders using indices for safe removal
        for (size_t i = 0; i < pendingOrders.size(); /* no increment */) {
            try {
                Order& order = pendingOrders[i]; // Get reference

                // --- Order Type Handling (Market Orders Only for now) ---
                // TODO: Add check for Limit/Stop orders - if order.requestedPrice > 0 etc.

                // --- Check if Opening or Closing ---
                Position* existingPosition = nullptr;
                auto posIt = positions.find(order.symbol);
                bool positionExists = (posIt != positions.end());
                if (positionExists) {
                    existingPosition = &(posIt->second);
                }

                bool isClosingOrder = false;
                if (positionExists &&
                    ((order.type == OrderType::SELL && existingPosition->size > 0) || // Selling with Long position
                     (order.type == OrderType::BUY && existingPosition->size < 0)))   // Buying with Short position
                {
                    isClosingOrder = true;
                }
                // Note: An order in the same direction as an existing position is treated as 'Open' (increasing size)

                // --- Delegate to Appropriate Handler ---
                if (isClosingOrder) {
                    if (existingPosition) { // Should always be true if isClosingOrder is true
                        executeCloseOrder(order, *existingPosition, currentBar);
                    } else {
                        // Should not happen based on logic, but handle defensively
                        Utils::logMessage("Broker Error: Inconsistent state - Closing order without existing position? ID: " + std::to_string(order.id));
                        rejectOrder(order, OrderStatus::REJECTED, currentBar);
                    }
                } else { // Order is to Open or Increase position
                    executeOpenOrder(order, currentBar);
                }

                // --- Remove Processed Order from Pending ---
                // Both executeOpenOrder and executeCloseOrder handle the final state transition
                // (FILLED or REJECTED) and notification. We just need to remove it here.
                pendingOrders[i] = std::move(pendingOrders.back());
                pendingOrders.pop_back();
                // Do NOT increment 'i', process the swapped element next iteration
            } catch (const std::exception& e) {
                Utils::logMessage("Broker::processOrders Error: Exception processing order: " + std::string(e.what()));
                // Skip this order and move to the next one
                pendingOrders[i] = std::move(pendingOrders.back());
                pendingOrders.pop_back();
            } catch (...) {
                Utils::logMessage("Broker::processOrders Error: Unknown exception processing order");
                // Skip this order and move to the next one
                pendingOrders[i] = std::move(pendingOrders.back());
                pendingOrders.pop_back();
            }
        } // End for loop
    } catch (const std::exception& e) {
        Utils::logMessage("Broker::processOrders Error: Exception caught: " + std::string(e.what()));
    } catch (...) {
        Utils::logMessage("Broker::processOrders Error: Unknown exception caught");
    }
}

// --- Position Info ---
const Position* Broker::getPosition(const std::string& symbol) const {
    auto it = positions.find(symbol);
    if (it != positions.end()) {
        return &(it->second); // Return pointer to the Position object
    }
    return nullptr; // Return null if no position exists for the symbol
}

const std::map<std::string, Position>& Broker::getAllPositions() const {
    return positions;
}

// --- History ---
const std::vector<Order>& Broker::getOrderHistory() const {
    return orderHistory;
}