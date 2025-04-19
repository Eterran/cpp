#include "BenchmarkStrategy.h"
#include "Broker.h"
#include "Config.h"
#include "Utils.h"
#include "Order.h"
#include <stdexcept>

 // // Trade 1: Buy 10 @100, Sell 10 @150 → profit 500
// Trade 2: Buy 10 @120, Sell 10 @160 → profit 400
// Trade 3: Buy 10 @160, Sell 10 @200 → profit 400
// Total PnL = 1300 on starting cash.
// // Hourly returns on trade bars:
// R1=(1500−1000)/1000=0.50, R2=(1600−1200)/1200≈0.33, R3=(2000−1600)/1600=0.25
// Mean≈0.36, StdDev≈0.10, √(365·24)≈93.7 → Sharpe≈(0.36/0.10)*93.7≈337.

BenchmarkStrategy::BenchmarkStrategy()
    : shares_(0.0), initialized_(false)
    , entryBar_(0), exitBar_(0), entered_(false), exited_(false), entryPrice_(0.0), entryOrderId_(0)
    // initialize extra trades
    , entryBar2_(2), exitBar2_(5), entered2_(false), exited2_(false), shares2_(0.0), entryPrice2_(0.0), entryOrderId2_(0)
    , entryBar3_(6), exitBar3_(9), entered3_(false), exited3_(false), shares3_(0.0), entryPrice3_(0.0), entryOrderId3_(0)
{}

void BenchmarkStrategy::init() {
    if (!config || !broker) {
        throw std::runtime_error("BenchmarkStrategy::init Error: Config or Broker not set");
    }
    // Load fixed bars from config
    entryBar_ = config->getNested<size_t>("/Strategy/ENTRY_BAR", 0);
    exitBar_ = config->getNested<size_t>("/Strategy/EXIT_BAR", (data ? data->size()-1 : 0));
    entered_ = false;
    exited_ = false;

    double startingValue = broker->getStartingCash();
    metrics_ = std::make_unique<TradingMetrics>(startingValue);
    if (data) metrics_->setTotalBars(data->size());
    Utils::logMessage("--- BenchmarkStrategy Initialized ---");
    Utils::logMessage("Entry bar: " + std::to_string(entryBar_) + ", Exit bar: " + std::to_string(exitBar_));
    // Initialize previous value for returns
    metrics_->updatePortfolioValue(startingValue);

    // Setup additional fixed trades
    entered2_ = exited2_ = false;
    entered3_ = exited3_ = false;
}

void BenchmarkStrategy::next(const Bar& currentBar, size_t currentBarIndex, const double currentPrices) {
    // Open position at entry bar
    if (!entered_ && currentBarIndex == entryBar_) {
        double price = currentBar.columns[1];
        shares_ = 10.0;
        Order entryOrder;
        entryOrder.type = OrderType::BUY;
        entryOrder.symbol = dataName;
        entryOrder.reason = OrderReason::ENTRY_SIGNAL;
        entryOrder.requestedSize = shares_;
        entryOrder.requestedPrice = price;
        entryOrderId_ = broker->submitOrder(entryOrder);
        // Execute fill immediately so entryPrice_ is set
        broker->processOrders(currentBar);
        entered_ = true;
        return;
    }
    // Close position at exit bar
    else if (entered_ && !exited_ && currentBarIndex == exitBar_) {
        double price = currentBar.columns[1];
        Order exitOrder;
        exitOrder.type = OrderType::SELL;
        exitOrder.symbol = dataName;
        exitOrder.reason = OrderReason::EXIT_SIGNAL;
        // Request full current position size
        if (auto pos = broker->getPosition(dataName)) exitOrder.requestedSize = std::abs(pos->size);
        else exitOrder.requestedSize = shares_;
        exitOrder.requestedPrice = price;
        int exitId = broker->submitOrder(exitOrder);
        // Fill exit immediately for metrics
        broker->processOrders(currentBar);
        Utils::logMessage("BenchmarkStrategy: Exit of order #" + std::to_string(exitId)
            + " at bar " + std::to_string(currentBarIndex));
        exited_ = true;
        return;
    }

    // Second trade at fixed bars
    if (!entered2_ && currentBarIndex == entryBar2_) {
        double price = currentBar.columns[1];
        shares2_ = 10.0;
        Order entryOrder2;
        entryOrder2.type = OrderType::BUY;
        entryOrder2.symbol = dataName;
        entryOrder2.reason = OrderReason::ENTRY_SIGNAL;
        entryOrder2.requestedSize = shares2_;
        entryOrder2.requestedPrice = price;
        entryOrderId2_ = broker->submitOrder(entryOrder2);
        broker->processOrders(currentBar);
        entered2_ = true;
        return;
    } else if (entered2_ && !exited2_ && currentBarIndex == exitBar2_) {
        double price = currentBar.columns[1];
        Order exitOrder2;
        exitOrder2.type = OrderType::SELL;
        exitOrder2.symbol = dataName;
        exitOrder2.reason = OrderReason::EXIT_SIGNAL;
        exitOrder2.requestedSize = shares2_;
        exitOrder2.requestedPrice = price;
        broker->submitOrder(exitOrder2);
        broker->processOrders(currentBar);
        exited2_ = true;
        return;
    }

    // Third trade at fixed bars
    if (!entered3_ && currentBarIndex == entryBar3_) {
        double price = currentBar.columns[1];
        shares3_ = 10.0;
        Order entryOrder3;
        entryOrder3.type = OrderType::BUY;
        entryOrder3.symbol = dataName;
        entryOrder3.reason = OrderReason::ENTRY_SIGNAL;
        entryOrder3.requestedSize = shares3_;
        entryOrder3.requestedPrice = price;
        entryOrderId3_ = broker->submitOrder(entryOrder3);
        broker->processOrders(currentBar);
        entered3_ = true;
        return;
    } else if (entered3_ && !exited3_ && currentBarIndex == exitBar3_) {
        double price = currentBar.columns[1];
        Order exitOrder3;
        exitOrder3.type = OrderType::SELL;
        exitOrder3.symbol = dataName;
        exitOrder3.reason = OrderReason::EXIT_SIGNAL;
        exitOrder3.requestedSize = shares3_;
        exitOrder3.requestedPrice = price;
        broker->submitOrder(exitOrder3);
        broker->processOrders(currentBar);
        exited3_ = true;
        return;
    }

    // Track metrics each bar
    double currentValue = broker->getValue(currentPrices);
    double previousValue = metrics_->getPreviousValue();
    if (previousValue > 0 && currentBarIndex > 0) {
        double periodReturn = (currentValue - previousValue) / previousValue;
        metrics_->recordReturn(periodReturn);
    }
    metrics_->updatePortfolioValue(currentValue);
}

void BenchmarkStrategy::stop() {
    // If position never exited, force a closing order to record metrics
    if (entered_ && !exited_ && data && !data->empty()) {
        // Build and submit exit order
        double lastPrice = data->back().columns[1];
        Order exitOrder;
        exitOrder.type = OrderType::SELL;
        exitOrder.symbol = dataName;
        exitOrder.reason = OrderReason::EXIT_SIGNAL;
        exitOrder.requestedSize = shares_;
        exitOrder.requestedPrice = lastPrice;
        broker->submitOrder(exitOrder);
        // Process orders on final bar to trigger notifyOrder
        broker->processOrders(data->back());
        exited_ = true;
    }
    // Compute final portfolio value
    double finalValue = 0.0;
    // if (data && !data->empty()) {
    //     std::map<std::string, double> prices{{dataName, data->back().columns[1]}};
    //     finalValue = broker->getValue(prices);
    // }
    if (data && !data->empty()) {
        double price = data->back().columns[1];
        finalValue = broker->getValue(price);
    }
    std::string report = metrics_->generateSummaryReport(finalValue, "BenchmarkStrategy");
    Utils::logMessage(report);
}

void BenchmarkStrategy::notifyOrder(const Order& order) {
    if (order.status == OrderStatus::FILLED) {
        if (order.reason == OrderReason::ENTRY_SIGNAL) {
            // Capture entry price for profit calc
            entryPrice_ = order.filledPrice;
            Utils::logMessage("BenchmarkStrategy: Entry filled @ " + std::to_string(entryPrice_));
        }
        else if (order.reason == OrderReason::EXIT_SIGNAL) {
            // Record commission and trade
            metrics_->recordCommission(order.commission);
            bool profitable = (order.filledPrice > entryPrice_);
            metrics_->recordTrade(profitable);
            Utils::logMessage("BenchmarkStrategy: Exit filled @ " + std::to_string(order.filledPrice)
                + (profitable ? " (PROFIT)" : " (LOSS)"));
        }
    }
}