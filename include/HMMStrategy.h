#pragma once
#include "Strategy.h"
#include "ModelInterface.h"
#include <vector>
#include "TradingMetrics.h"

class HMMStrategy : public Strategy {
protected:
    std::unique_ptr<ModelInterface> hmm_model_;
    std::vector<std::unique_ptr<ModelInterface>> regime_models_;
    double entryThreshold_;
    // double stopLossPips_;
    // double takeProfitPips_;
    double pipValue_;
    bool inPosition_;
    double entryPrice_;
    int currentRegime_;
    int n_components_;

    std::unique_ptr<TradingMetrics> metrics;
public:
    HMMStrategy();
    std::string getName() const override;
    void init() override;
    void next(const Bar& currentBar, size_t currentBarIndex, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;

protected:
    /**
     * Hook for mapping a model prediction and price into trade orders.
     * Default implementation: if no position and pred > threshold, submit a BUY with TP/SL.
     */
    virtual void handlePrediction(float prediction, double price);
};