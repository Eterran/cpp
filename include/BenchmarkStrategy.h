#ifndef BENCHMARKSTRATEGY_H
#define BENCHMARKSTRATEGY_H

#include "Strategy.h"
#include "TradingMetrics.h"
#include "Bar.h"
#include "Utils.h"
#include "Broker.h"
#include "Config.h"
#include <map>
#include <memory>

class BenchmarkStrategy : public Strategy {
private:
    std::unique_ptr<TradingMetrics> metrics_;
    double shares_;
    bool initialized_;
    size_t entryBar_;
    size_t exitBar_;
    bool entered_;
    bool exited_;
    double entryPrice_;  // store fill price for exit profit check
    int entryOrderId_;   // track ID of entry order to reuse for exit

    // Fixed second trade
    size_t entryBar2_;
    size_t exitBar2_;
    bool entered2_;
    bool exited2_;
    double shares2_;
    double entryPrice2_;
    int entryOrderId2_;

    // Fixed third trade
    size_t entryBar3_;
    size_t exitBar3_;
    bool entered3_;
    bool exited3_;
    double shares3_;
    double entryPrice3_;
    int entryOrderId3_;

public:
    BenchmarkStrategy();
    virtual std::string getName() const override { return "BenchmarkStrategy"; }
    void init() override;
    // void next(const Bar& currentBar, size_t currentBarIndex, const std::map<std::string, double>& currentPrices) override;
    void next(const Bar& currentBar, size_t currentBarIndex, const double currentPrice) override;
    void stop() override;
    void notifyOrder(const Order& order) override;
};

#endif // BENCHMARKSTRATEGY_H