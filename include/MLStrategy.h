// #pragma once
// #include "Strategy.h"
// #include "ModelInterface.h"
// #include <vector>
// #include <memory>

// class MLStrategy : public Strategy {
// // Change member visibility to allow subclasses to access thresholds and state
// protected:
//     std::unique_ptr<ModelInterface> hmm_model_;
//     std::vector<std::unique_ptr<ModelInterface>> regime_models_;
//     double entryThreshold_;
//     double stopLossPips_;
//     double takeProfitPips_;
//     double pipValue_;
//     bool inPosition_;
//     double entryPrice_;
//     int currentRegime_;
// public:
//     MLStrategy();
//     std::string getName() const override { return "MLStrategy"; }
//     void init() override;
//     void next(const Bar& currentBar, size_t currentBarIndex, const std::map<std::string, double>& currentPrices) override;
//     void stop() override;
//     void notifyOrder(const Order& order) override;

// protected:
//     /**
//      * Hook for mapping a model prediction and price into trade orders.
//      * Default implementation: if no position and pred > threshold, submit a BUY with TP/SL.
//      */
//     virtual void handlePrediction(float prediction, double price);
// };

// // Example subclass showing how to override handlePrediction:
// class ExampleMLStrategy : public MLStrategy {
// public:
//     ExampleMLStrategy() {}
//     std::string getName() const override { return "ExampleMLStrategy"; }

// protected:
//     /**
//      * Example override: go long if pred > threshold, go short if pred < -threshold.
//      */
//     void handlePrediction(float prediction, double price) override;
// };