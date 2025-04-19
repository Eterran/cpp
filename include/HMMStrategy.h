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
    double pipValue_;
    bool inPosition_;
    double entryPrice_;
    int currentRegime_;
    int previousRegime_;
    int positionType_; // 1 for long, -1 for short, 0 for none
    int n_components_;
    float lastPrediction_;
    double trailStopPrice_;
    std::vector<Bar> barHistory_;  // Store recent bars for sequence-based prediction
    std::vector<Bar> allBarHistory_;  // Complete bar history for global predictions
    std::map<int, double> regimeVolatility_; // Store volatility levels for each regime

    std::unique_ptr<TradingMetrics> metrics;
public:
    HMMStrategy();
    std::string getName() const override;
    void init() override;
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
    
    /**
     * Calculate position size based on prediction strength and risk parameters
     */
    double calculatePositionSize(float predictionStrength, int regime, double price);
    
    /**
     * Calculate volatility for the current regime
     */
    double calculateRegimeVolatility(int regime, size_t lookback = 20);
    
    /**
     * Update trailing stop loss
     */
    void updateTrailStop(double currentPrice);
    
    /**
     * Check if we should exit a position
     */
    bool shouldExitPosition(int regime, float prediction, double currentPrice);
};