// Strategy.h
#ifndef STRATEGY_H
#define STRATEGY_H

#include "Bar.h"
#include "Order.h"
// #include "Broker.h" // Forward declare instead to avoid circular include
#include <vector>
#include <string>
#include <map> // Include map for passing current prices

// Forward declarations
class Broker; // Let Strategy know Broker exists
struct Config; // Let Strategy know Config exists (defined in Phase 4)

class Strategy {
protected:
    Broker* broker; // Non-owning pointer to the broker instance
    const std::vector<Bar>* data; // Non-owning pointer to the historical data
    std::string dataName; // Name of the data series (e.g., "USDJPY")
    Config* config; // Non-owning pointer to configuration settings

public:
    virtual std::string getName() const;
    Strategy() : broker(nullptr), data(nullptr), config(nullptr) {} // Default init
    virtual ~Strategy() = default; // Virtual destructor

    // --- Setup Methods (called by Engine) ---
    virtual void setBroker(Broker* b) { broker = b; }
    virtual void setData(const std::vector<Bar>* d, const std::string& name) { data = d; dataName = name; }
    virtual void setConfig(Config* cfg) { config = cfg; }

    // --- Core Strategy Lifecycle Methods (to be overridden) ---
    // Called once before the backtest loop starts
    virtual void init() = 0;
    // Called for each bar of data after broker processing for that bar
    virtual void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) = 0;
    // Called once after the backtest loop finishes
    virtual void stop() = 0;
    // Called by the Broker when an order status changes
    virtual void notifyOrder(const Order& order) = 0;

};

#endif // STRATEGY_H