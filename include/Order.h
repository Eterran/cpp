// Order.h
#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <chrono>
#include <limits> // For numeric_limits

enum class OrderType {
    BUY,
    SELL
};

enum class OrderStatus {
    CREATED,    // Initial state before broker processing
    SUBMITTED,  // Handed to broker simulation
    ACCEPTED,   // Basic checks passed (e.g., positive size) - Optional intermediate state
    FILLED,     // Successfully executed
    CLOSED,
    CANCELLED,  // Order cancelled before filling
    REJECTED,   // Broker rejected (e.g., insufficient funds/margin)
    MARGIN      // Rejected specifically due to margin
};

enum class OrderReason {
    ENTRY_SIGNAL,
    EXIT_SIGNAL,
    STOP_LOSS,
    TAKE_PROFIT,
    BANKRUPTCY_PROTECTION,
    MANUAL_CLOSE
};


struct Order {
    int id = -1; // Unique ID assigned by Broker
    OrderType type = OrderType::BUY;
    OrderStatus status = OrderStatus::CREATED;
    OrderReason reason = OrderReason::ENTRY_SIGNAL; // Default reason
    std::string symbol = "";          // e.g., "USDJPY", "BTCUSDT"
    double requestedSize = 0.0;       // Absolute value of units/shares requested
    double filledSize = 0.0;          // Actual size filled (can differ in reality)
    double requestedPrice = 0.0;      // For Limit/Stop orders (0.0 for Market)
    double filledPrice = 0.0;         // Average price at which the order was filled
    double commission = 0.0;          // Commission charged for this order execution
    double takeProfit = 0.0;          // Price at which to take profit (0.0 if not set)
    double stopLoss = 0.0;            // Price at which to stop loss (0.0 if not set)
    std::chrono::system_clock::time_point creationTime{};
    std::chrono::system_clock::time_point executionTime{};
    // int strategyId = -1; // Optional: If multiple strategies run concurrently

    // Helper to check if order is in a final state
    bool isClosed() const {
        return status == OrderStatus::FILLED ||
               status == OrderStatus::CANCELLED ||
               status == OrderStatus::REJECTED ||
               status == OrderStatus::MARGIN;
    }
};

#endif // ORDER_H