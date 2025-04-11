// Broker.h
#ifndef BROKER_H
#define BROKER_H

#include "Order.h"
#include "Position.h"
#include "Bar.h" // Needed for processOrders argument
#include <vector>
#include <map>
#include <string>
#include <memory> // Maybe for future use, not strictly needed now

// Forward declaration of Strategy class to break circular dependency
class Strategy;

class Broker {
private:
    double startingCash;
    double cash;
    double leverage;
    double commissionRate; // Commission per unit traded (adjust if % based)
    std::map<std::string, Position> positions; // Map symbol to open Position
    std::vector<Order> pendingOrders;
    std::vector<Order> orderHistory;
    std::map<std::string, double> pointValues; // Store point values per symbol
    int nextOrderId;
    Strategy* strategy; // Pointer to the strategy for notifications (non-owning)

    // Helper to get the relevant price from a bar for filling market orders
    double getFillPrice(const Bar& bar, OrderType orderType) const;

    // Helper to calculate margin required for an order
    double calculateMarginNeeded(double size, double price) const;

    // Helper to calculate commission for an order
    double calculateCommission(double size) const;

    // Helper to find the point value for a symbol
    double getPointValue(const std::string& symbol) const;

public:
    // Add a default constructor 
    Broker();
    Broker(double initialCash, double lev, double commRate); // CommRate e.g., 0.00002 per unit

    // Set the strategy instance (called by engine)
    void setStrategy(Strategy* strat);

    // --- Account Info ---
    double getStartingCash() const;
    double getCash() const;
    double getValue(const std::map<std::string, double>& currentPrices); // Calculate total portfolio value

    // --- Order Management ---
    // Creates an order and adds it to pending queue. Returns the order ID.
    int submitOrder(OrderType type, OrderReason reason, const std::string& symbol, double size);

    // Processes pending orders based on the current market data bar.
    // Notifies strategy of outcomes.
    void processOrders(const Bar& currentBar);

    // --- Position Info ---
    const Position* getPosition(const std::string& symbol) const; // Get const pointer
    const std::map<std::string, Position>& getAllPositions() const; // Get reference to all positions

    // --- Configuration ---
    void setPointValue(const std::string& symbol, double points);

    // --- History ---
    const std::vector<Order>& getOrderHistory() const;
};

#endif // BROKER_H