// Broker.cpp
#include "Broker.h"
#include "Strategy.h" // Include Strategy definition now
#include "Utils.h"    // For logging and pip point calculation
#include <cmath>      // For std::abs, std::round etc.
#include <stdexcept>  // For potential errors (though mostly handled via logging/rejection)
#include <numeric>    // For std::accumulate

// --- Constructor ---
Broker::Broker(double initialCash, double lev, double commRate) :
    startingCash(initialCash),
    cash(initialCash),
    leverage(lev > 0 ? lev : 1.0), // Ensure leverage is at least 1
    commissionRate(commRate),
    nextOrderId(1), // Start order IDs from 1
    strategy(nullptr) // Initialize strategy pointer to null
{
    if (initialCash <= 0) {
        Utils::logMessage("Broker Warning: Initial cash is zero or negative.");
        // Consider throwing an error if this is invalid state
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
{
    Utils::logMessage("Broker initialized with default values. Start Cash: 10000, Leverage: 1.0, Commission: 0.0");
}

// --- Configuration ---
void Broker::setStrategy(Strategy* strat) {
    strategy = strat;
}

void Broker::setPointValue(const std::string& symbol, double points) {
    pointValues[symbol] = points;
    Utils::logMessage("Broker: Set point value for " + symbol + " to " + std::to_string(points));
}

// --- Helpers ---
double Broker::getFillPrice(const Bar& bar, OrderType orderType) const {
    // More realistic fill logic attempt:
    if (orderType == OrderType::BUY) {
        // Buyer gets the ASK price (higher)
        return (bar.ask > 0) ? bar.ask : bar.close; // Use ask if available, else close
    } else { // SELL
        // Seller gets the BID price (lower)
        return (bar.bid > 0) ? bar.bid : bar.close; // Use bid if available, else close
    }
    // Original simple logic: return bar.close;
}

double Broker::calculateMarginNeeded(double size, double price) const {
    if (leverage <= 0) return std::abs(size * price); // No leverage, full amount needed
    return std::abs(size * price) / leverage;
}

double Broker::calculateCommission(double size) const {
    // Commission based on rate per unit traded
    return std::abs(size) * commissionRate;
}

double Broker::getPointValue(const std::string& symbol) const { // Keep it const
    auto it = pointValues.find(symbol);
    if (it != pointValues.end()) {
        return it->second; // Return stored value
    }
    // Point value not found in map, calculate default but DO NOT store it here
    Utils::logMessage("Broker Warning: Point value not explicitly set for symbol '" + symbol + "'. Using default calculation.");
    return Utils::calculatePipPoint(symbol); // Return calculated default (function name needs updating in Utils)
}


// --- Account Info ---
double Broker::getStartingCash() const {
    return startingCash;
}

double Broker::getCash() const {
    return cash;
}

double Broker::getValue(const std::map<std::string, double>& currentPrices) {
    double totalValue = cash;
    double positionsValue = 0.0;

    for (const auto& pair : positions) {
        const std::string& symbol = pair.first;
        const Position& pos = pair.second;

        auto priceIt = currentPrices.find(symbol);
        if (priceIt != currentPrices.end()) {
            positionsValue += pos.calculateUnrealizedPnL(priceIt->second);
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


// --- Order Management ---
int Broker::submitOrder(OrderType type, OrderReason reason, const std::string& symbol, double size) {
    if (size <= 0) {
        Utils::logMessage("Broker Error: Order size must be positive. Requested: " + std::to_string(size));
        // Cannot create a valid order object to return status easily here.
        // Returning -1 to indicate immediate failure.
        return -1;
    }
    if (strategy == nullptr) {
         Utils::logMessage("Broker Error: Strategy not set. Cannot submit order.");
         return -1;
    }

    Order order;
    order.id = nextOrderId++;
    order.type = type;
    order.status = OrderStatus::SUBMITTED; // Mark as submitted to broker simulation
    order.reason = reason;
    order.symbol = symbol;
    order.requestedSize = size; // Store positive size always
    order.creationTime = std::chrono::system_clock::now(); // Use current time for creation

    pendingOrders.push_back(order);
    // Utils::logMessage("Broker: Order " + std::to_string(order.id) + " submitted: " + (type == OrderType::BUY ? "BUY" : "SELL") + " " + std::to_string(size) + " " + symbol);

    // Notify strategy immediately that order was submitted? Optional. Backtrader doesn't always.
    // strategy->notifyOrder(order);

    return order.id;
}

void Broker::processOrders(const Bar& currentBar) {
    if (strategy == nullptr) return; // Cannot process without strategy for notifications

    auto it = pendingOrders.begin();
    while (it != pendingOrders.end()) {
        Order& order = *it; // Work with the order directly

        // Assume Market Order Fill Logic for now
        double fillPrice = getFillPrice(currentBar, order.type);
        if (fillPrice <= 0) {
            Utils::logMessage("Broker Warning: Invalid fill price (" + std::to_string(fillPrice) + ") for Bar Time: " + Utils::formatTimestamp(currentBar.timestamp) + ". Cannot process order " + std::to_string(order.id)); 
            // Decide: Reject order or wait for next bar? Reject for now.
            order.status = OrderStatus::REJECTED;
            order.executionTime = currentBar.timestamp; // Use bar's time
            orderHistory.push_back(order);
            strategy->notifyOrder(order);
            it = pendingOrders.erase(it); // Remove from pending
            continue;
        }


        double orderValue = order.requestedSize * fillPrice;
        double marginNeeded = calculateMarginNeeded(order.requestedSize, fillPrice);
        double commission = calculateCommission(order.requestedSize);

        // --- Check if closing an existing position ---
        bool isClosingOrder = false;
        Position* existingPosition = nullptr;
        auto posIt = positions.find(order.symbol);
        if (posIt != positions.end()) {
            existingPosition = &(posIt->second);
            // Check if order type is opposite to position direction
            if ((order.type == OrderType::SELL && existingPosition->size > 0) ||
                (order.type == OrderType::BUY && existingPosition->size < 0))
            {
                // Check if size matches or exceeds existing position size (allow partial close later?)
                if (order.requestedSize >= std::abs(existingPosition->size)) {
                    isClosingOrder = true;
                    // Adjust order size to exactly match position size if it exceeds it
                    // This prevents accidentally opening an opposite position immediately
                    // Note: In reality, overfills might happen or partial fills. Simple model assumes exact fill up to requested size.
                    order.requestedSize = std::abs(existingPosition->size);
                    orderValue = order.requestedSize * fillPrice; // Recalculate value based on adjusted size
                    commission = calculateCommission(order.requestedSize); // Recalculate commission
                    // Margin check might not be needed for *closing* trades in some brokers, but check anyway for consistency
                    marginNeeded = 0; // Typically no additional margin for closing
                } else {
                    // TODO: Handle partial closes if needed. For now, assume full close orders.
                    Utils::logMessage("Broker Info: Order " + std::to_string(order.id) + " is a partial close. Not implemented, treating as new position attempt.");
                }
            }
        }


        // --- Perform Checks (Margin & Cash) ---
        bool checksPassed = true;
        if (!isClosingOrder) { // Only check margin/cash for opening new positions
            if (marginNeeded > cash) { // Check margin FIRST
                Utils::logMessage("Broker: Order " + std::to_string(order.id) + " REJECTED (Margin). Needed: " + std::to_string(marginNeeded) + ", Available Cash: " + std::to_string(cash));
                order.status = OrderStatus::MARGIN;
                checksPassed = false;
            } else if (commission > cash - marginNeeded) { // Check if cash covers commission AFTER reserving margin
                Utils::logMessage("Broker: Order " + std::to_string(order.id) + " REJECTED (Cash for Commission). Commission: " + std::to_string(commission) + ", Cash after Margin: " + std::to_string(cash - marginNeeded));
                order.status = OrderStatus::REJECTED; // Generic rejection if not margin specific
                checksPassed = false;
            }
            // Note: A more realistic check would be marginNeeded + commission <= cash,
            // or even stricter depending on broker rules. Let's use the stricter check.
            /*
            if (marginNeeded + commission > cash) {
                Utils::logMessage("Broker: Order " + std::to_string(order.id) + " REJECTED (Margin + Commission). Needed: " + std::to_string(marginNeeded + commission) + ", Cash: " + std::to_string(cash));
                order.status = OrderStatus::MARGIN; // Or REJECTED
                checksPassed = false;
            }
            */

        } else {
            // Closing order: check if cash covers commission
            if (commission > cash) {
                Utils::logMessage("Broker: Order " + std::to_string(order.id) + " REJECTED (Cash for Commission on Closing). Needed: " + std::to_string(commission) + ", Cash: " + std::to_string(cash));
                order.status = OrderStatus::REJECTED;
                checksPassed = false;
            }
        }


        // --- Process Order Fill/Rejection ---
        if (checksPassed) {
            // Fill the order
            order.status = OrderStatus::FILLED;
            order.filledPrice = fillPrice;
            order.filledSize = order.requestedSize; // Assume full fill for simplicity
            order.commission = commission;
            order.executionTime = currentBar.timestamp; // Use bar's timestamp for execution time

            // Update cash
            cash -= commission; // Deduct commission always

            double pnl = 0.0; // Profit/Loss realized on this trade if closing

            if (isClosingOrder && existingPosition) {
                // Realize PnL
                pnl = (fillPrice - existingPosition->entryPrice) * existingPosition->size;
                cash += pnl; // Add realized P/L to cash

                // Add back margin released (simplified: add back margin used at entry)
                // double marginAtEntry = calculateMarginNeeded(existingPosition->size, existingPosition->entryPrice);
                // cash += marginAtEntry; // This part is tricky and depends on exact margin model. Omit for simplicity now.

                Utils::logMessage("Broker: Closing position " + order.symbol + ". Size: " + std::to_string(existingPosition->size)
                                     + ", Entry: " + std::to_string(existingPosition->entryPrice)
                                     + ", Exit: " + std::to_string(fillPrice)
                                     + ", PnL: " + std::to_string(pnl)
                                     + ", Commission: " + std::to_string(commission));

                // Remove position
                positions.erase(order.symbol);

            } else { // Opening new position or increasing existing one (not handled yet)
                if (existingPosition) {
                    // Increasing position size - TODO if needed
                    Utils::logMessage("Broker Warning: Increasing position size not fully implemented.");
                    // Simple addition for now:
                    // cash -= (order.type == OrderType::BUY ? orderValue : -orderValue); // Adjust cash by value only if NOT using margin for this part? Confusing.
                    // Recalculate average entry price?
                    // existingPosition->entryPrice = ((existingPosition->size * existingPosition->entryPrice) + (order.filledSize * order.filledPrice)) / (existingPosition->size + order.filledSize);
                    // existingPosition->size += order.filledSize; // Adjust size
                } else {
                    // Create new position
                    Position newPos;
                    newPos.symbol = order.symbol;
                    // Size is positive for BUY, negative for SELL
                    newPos.size = (order.type == OrderType::BUY) ? order.filledSize : -order.filledSize;
                    newPos.entryPrice = fillPrice;
                    newPos.entryTime = order.executionTime;
                    newPos.pipPoint = getPointValue(order.symbol); // Store point value in position
                    newPos.lastValue = std::abs(newPos.size * newPos.entryPrice); // Store initial value

                    positions[order.symbol] = newPos;

                    // Margin is held implicitly, cash doesn't change directly by position value here, only by commission.
                    Utils::logMessage("Broker: Opening position " + order.symbol + ". Size: " + std::to_string(newPos.size)
                                         + ", Entry: " + std::to_string(fillPrice)
                                         + ", Commission: " + std::to_string(commission));
                }
            }

            // Add to history and notify
            orderHistory.push_back(order);
            strategy->notifyOrder(order);
            it = pendingOrders.erase(it); // Remove from pending

        } else { // Checks failed
            // Update status, add to history, notify, remove from pending
            order.executionTime = currentBar.timestamp; // Mark time of rejection
            orderHistory.push_back(order);
            strategy->notifyOrder(order);
            it = pendingOrders.erase(it); // Remove from pending
        }

    } // End while loop through pending orders
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