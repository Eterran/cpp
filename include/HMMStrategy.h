#pragma once
#include "Strategy.h"
#include "ModelInterface.h"
#include <vector>
#include "TradingMetrics.h"
#include <map>
#include <cmath>

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
    std::vector<Bar> barHistory_;  // Store recent bars for sequence-based prediction
    std::vector<Bar> allBarHistory_;  // Complete bar history for global predictions

    std::unique_ptr<TradingMetrics> metrics;
public:
    HMMStrategy();
    std::string getName() const override;
    void init() override;
    // void next(const Bar& currentBar, size_t currentBarIndex, const std::map<std::string, double>& currentPrice) override;
    void next(const Bar& currentBar, size_t currentBarIndex, const double currentPrice) override;
    void stop() override;
    void notifyOrder(const Order& order) override;

protected:
    /**
     * Hook for mapping a model prediction and price into trade orders.
     * Default implementation: if no position and pred > threshold, submit a BUY with TP/SL.
     */
    virtual void handlePrediction(int regime);
    
    /**
     * Normalize features using z-score normalization (subtract mean, divide by std)
     */
    std::vector<std::vector<float>> normalizeFeatures(const std::vector<std::vector<float>>& features);
};