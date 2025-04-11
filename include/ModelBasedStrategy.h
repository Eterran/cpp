// include/ModelBasedStrategy.h
#ifndef MODELBASEDSTRATEGY_H
#define MODELBASEDSTRATEGY_H

#include "Strategy.h"
#include "ISignalGenerator.h" // Include the interface
#include "Position.h"
#include "Config.h"
#include "Utils.h"
#include <memory> // For unique_ptr

class ModelBasedStrategy : public Strategy {
private:
    // --- Signal Generator ---
    std::unique_ptr<ISignalGenerator> signalGenerator;

    // --- Parameters for Signal Interpretation & Execution ---
    double longThreshold = 0.1;   // Example: Enter long if predictedValue > 0.1
    double shortThreshold = -0.1; // Example: Enter short if predictedValue < -0.1
    double exitThresholdMultiplier = 0.5; // Example: Exit if prediction reverses beyond 0.5 * entry threshold
    double slPips = 100.0;         // Default SL pips (can be overridden by config)
    double tpPips = 100.0;         // Default TP pips (can be overridden by config)
    bool useFixedSlTp = true;      // Use fixed SL/TP pips from params, ignore model's potential output for this

    // Sizing parameters
    std::string positionType = "fixed";
    double fixedSize = 1.0;
    double cashPercent = 2.0;
    double riskPercent = 1.0;

    // General parameters
    bool bankruptcyProtection = true;
    double forceExitPercent = -50.0;
    bool debugMode = false;

    // --- State ---
    Position currentPosition;
    bool inPosition = false;
    int currentPendingOrderId = -1;
    double pipPoint = 0.0001;
    double startingAccountValue = 0.0;
    bool bankrupt = false;
    int tradeCount = 0;
    int profitableTrades = 0;

    // Helper for sizing
    double calculateSize(double currentPrice, double accountValue, double stopLossPips); // Pass SL for risk calc

public:
    ModelBasedStrategy(); // Constructor
    ~ModelBasedStrategy() override = default;

    // --- Overridden Lifecycle Methods ---
    void init() override;
    // Pass index instead of bar? Need bar for price checks/logging. Pass both?
    // Let's keep Bar for now, engine passes index to generator.
    void next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) override;
    void stop() override;
    void notifyOrder(const Order& order) override;

private:
    // Factory function to create signal generator based on config
    std::unique_ptr<ISignalGenerator> createSignalGenerator(const Config* config);
};

#endif // MODELBASEDSTRATEGY_H