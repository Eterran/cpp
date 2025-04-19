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
#include <random> // For slippage generation

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
    int nextOrderId;
    Strategy* strategy; // Pointer to the strategy for notifications (non-owning)
    std::mt19937 rng; // Random number generator for slippage
    // std::uniform_real_distribution<double> slippageDist; // Distribution for slippage percentage

    // --- Private Helpers ---
    double getFillPrice(const Bar& bar, OrderType orderType, double requestedPrice = 0.0) const;
    double calculateMarginNeeded(double size, double price) const;
    double calculateCommission(double size, double price) const;
    double getPointValue(const std::string& symbol) const; // Renamed from getPointValue

    // NEW: Handles opening/increasing a position
    void executeOpenOrder(Order& order, const Bar& executionBar);

    // NEW: Handles closing/reducing/reversing a position
    void executeCloseOrder(Order& order, Position& existingPosition, const Bar& executionBar);

    // Helper for handling rejected orders
    void rejectOrder(Order& order, OrderStatus rejectionStatus, const Bar& executionBar);

    // Check if positions hit take profit or stop loss levels
    void checkTakeProfitStopLoss(const Bar& currentBar, const std::map<std::string, double>& currentPrices);

    // Apply random slippage to price
    // double applySlippage(double basePrice, bool isFavorable);

public:
    // Add a default constructor
    Broker();
    Broker(double initialCash, double lev, double commRate); // CommRate e.g., 0.00002 per unit
    ~Broker() = default;

    // Set the strategy instance (called by engine)
    void setStrategy(Strategy* strat);

    // --- Account Info ---
    double getStartingCash() const;
    double getCash() const;
    double getValue(const std::map<std::string, double>& currentPrices); // Calculate total portfolio value
    double getValue(const double currentPrices); // Calculate total portfolio value

    // --- Order Management ---
    // Creates an order and adds it to pending queue. Returns the order ID.
    int submitOrder(Order order);

    // Processes pending orders based on the current market data bar.
    // Notifies strategy of outcomes.
    void processOrders(const Bar& currentBar);

    // --- Position Info ---
    const Position* getPosition(const std::string& symbol) const; // Get const pointer
    const std::map<std::string, Position>& getAllPositions() const; // Get reference to all positions

    // --- Configuration ---

    // --- History ---
    const std::vector<Order>& getOrderHistory() const;
};

#endif // BROKER_H