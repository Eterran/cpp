// src/ModelBasedStrategy.cpp
#include "ModelBasedStrategy.h"
#include "OnnxSignalGenerator.h" // Include concrete generator(s)
// #include "RuleBasedSignalGenerator.h" // If you create others later
#include "Broker.h"
#include "Utils.h"
#include <cmath>
#include <stdexcept>
#include <iomanip>

ModelBasedStrategy::ModelBasedStrategy() {} // Default constructor

// --- Factory for Signal Generator ---
std::unique_ptr<ISignalGenerator> ModelBasedStrategy::createSignalGenerator(const Config* config) {
    std::string generatorType = config->getNested<std::string>("/Strategy/SIGNAL_GENERATOR_TYPE", "ONNX"); // Default to ONNX

    Utils::logMessage("ModelBasedStrategy: Creating signal generator of type: " + generatorType);

    if (generatorType == "ONNX") {
        return std::make_unique<OnnxSignalGenerator>();
    }
    // else if (generatorType == "SmaCrossRule") { // Example for future
    //     return std::make_unique<SmaCrossSignalGenerator>();
    // }
    else {
        Utils::logMessage("ModelBasedStrategy Error: Unknown SIGNAL_GENERATOR_TYPE: " + generatorType);
        return nullptr; // Return null indicates failure
    }
}


// --- Initialization ---
void ModelBasedStrategy::init() {
    if (!config || !broker || !data) { // Check data pointer too
        throw std::runtime_error("ModelBasedStrategy::init Error: Config, Broker, or Data not set.");
    }

    // --- Load Strategy Parameters ---
    try {
        longThreshold = config->getNested<double>("/Strategy/Interpretation/LONG_THRESHOLD", 0.1);
        shortThreshold = config->getNested<double>("/Strategy/Interpretation/SHORT_THRESHOLD", -0.1);
        exitThresholdMultiplier = config->getNested<double>("/Strategy/Interpretation/EXIT_MULTIPLIER", 0.5);

        useFixedSlTp = config->getNested<bool>("/Strategy/Execution/USE_FIXED_SLTP", true);
        slPips = config->getNested<double>("/Strategy/Execution/SL_PIPS", 100.0);
        tpPips = config->getNested<double>("/Strategy/Execution/TP_PIPS", 100.0);

        positionType = config->getNested<std::string>("/Strategy/Sizing/POSITION_TYPE", "fixed");
        fixedSize = config->getNested<double>("/Strategy/Sizing/FIXED_SIZE", 1.0);
        cashPercent = config->getNested<double>("/Strategy/Sizing/CASH_PERCENT", 2.0);
        riskPercent = config->getNested<double>("/Strategy/Sizing/RISK_PERCENT", 1.0);

        bankruptcyProtection = config->getNested<bool>("/Strategy/BANKRUPTCY_PROTECTION", true);
        forceExitPercent = config->getNested<double>("/Strategy/FORCE_EXIT_PERCENT", -50.0);
        debugMode = config->getNested<bool>("/Strategy/DEBUG_MODE", false);

    } catch (const std::exception& e) {
        Utils::logMessage("ModelBasedStrategy::init Warning: Error loading config parameter: " + std::string(e.what()) + ". Using defaults.");
        // Reset params to defaults
        longThreshold = 0.1; shortThreshold = -0.1; exitThresholdMultiplier = 0.5;
        slPips = 100.0; tpPips = 100.0; useFixedSlTp = true;
        positionType = "fixed"; fixedSize = 1.0; /*...*/
    }

    // --- Create and Initialize Signal Generator ---
    signalGenerator = createSignalGenerator(config);
    if (!signalGenerator) {
        throw std::runtime_error("ModelBasedStrategy::init Error: Failed to create Signal Generator.");
    }
    if (!signalGenerator->init(config, data)) { // Pass historical data pointer
         throw std::runtime_error("ModelBasedStrategy::init Error: Failed to initialize Signal Generator.");
    }


    // --- Set up State ---
    pipPoint = Utils::calculatePipPoint(dataName);
    broker->setPipPoint(dataName, pipPoint);
    startingAccountValue = broker->getCash();
    // ... reset other state vars ...
    inPosition = false; bankrupt = false; currentPendingOrderId = -1; tradeCount=0; profitableTrades=0;

    Utils::logMessage("--- ModelBasedStrategy Initialized ---");
    // ... Log loaded parameters ...
    Utils::logMessage("------------------------------------");
}

// --- Per-Bar Logic ---
void ModelBasedStrategy::next(const Bar& currentBar, const std::map<std::string, double>& currentPrices) {
    if (bankrupt || currentPendingOrderId != -1 || !signalGenerator) return;

    // Assume engine passes correct index corresponding to currentBar
    size_t currentBarIndex = ¤tBar - &historyData->front(); // Calculate index (requires historyData pointer)

    // --- Generate Signal ---
    TradingSignal signal = signalGenerator->generateSignal(currentBarIndex);

    // --- Interpret Signal & Execute ---
    double accountValue = broker->getValue(currentPrices);
    double currentPrice = currentBar.close; // Price for checks/logging
    double currentCheckPrice = currentBar.close; // Price for SL/TP checks (Bid/Ask adjusted)
     if(inPosition && currentPosition.size > 0 && currentBar.bid > 0) currentCheckPrice = currentBar.bid;
     else if (inPosition && currentPosition.size < 0 && currentBar.ask > 0) currentCheckPrice = currentBar.ask;
     //... etc fallback


    // --- Position Management (Check Exits) ---
    if (inPosition) {
        // Check Bankruptcy (same as RandomStrategy)
        // ...

        bool exitTriggered = false;
        OrderReason exitReason = OrderReason::MANUAL_CLOSE;
        OrderType exitOrderType = (currentPosition.size > 0) ? OrderType::SELL : OrderType::BUY;

        // Check Fixed SL/TP if enabled
        if (useFixedSlTp) {
             // Check TP (same as RandomStrategy using currentCheckPrice)
             if (currentPosition.takeProfitPrice > 0) { /* ... set exitTriggered, exitReason = TAKE_PROFIT ... */ }
             // Check SL (same as RandomStrategy using currentCheckPrice)
             if (!exitTriggered && currentPosition.stopLossPrice > 0) { /* ... set exitTriggered, exitReason = STOP_LOSS ... */ }
        } else {
            // TODO: Use SL/TP values potentially provided in 'signal' if model outputs them
        }

        // Optional: Check for Exit Signal based on prediction reversal
        if (!exitTriggered) {
             bool exitSignal = false;
             if (currentPosition.size > 0 && signal.predictedValue < shortThreshold * exitThresholdMultiplier) {
                  // Long position, but prediction turned strongly negative
                  exitSignal = true;
                  exitReason = OrderReason::EXIT_SIGNAL; // Generic exit signal
             } else if (currentPosition.size < 0 && signal.predictedValue > longThreshold * exitThresholdMultiplier) {
                  // Short position, but prediction turned strongly positive
                  exitSignal = true;
                  exitReason = OrderReason::EXIT_SIGNAL;
             }
             if(exitSignal) {
                  exitTriggered = true;
                  if(debugMode) Utils::logMessage("DEBUG: Model Exit Signal Triggered. Prediction: " + std::to_string(signal.predictedValue));
             }
        }


        // Submit Exit Order if Triggered
        if (exitTriggered) {
            currentPendingOrderId = broker->submitOrder(exitOrderType, exitReason, dataName, std::abs(currentPosition.size));
            if (currentPendingOrderId == -1) Utils::logMessage("  ERROR: Failed to submit exit order!");
            return;
        }
    }
    // --- Entry Logic ---
    else { // Not in position
        OrderType entryOrderType = OrderType::BUY; // Placeholder
        bool entrySignal = false;

        if (signal.predictedValue > longThreshold) {
            entrySignal = true;
            entryOrderType = OrderType::BUY;
        } else if (signal.predictedValue < shortThreshold) {
            entrySignal = true;
            entryOrderType = OrderType::SELL;
        }

        if (entrySignal) {
             if(debugMode) Utils::logMessage("DEBUG: Model Entry Signal. Prediction: " + std::to_string(signal.predictedValue));
             // Calculate size using the strategy's sizing parameters
             double stopPips = useFixedSlTp ? slPips : 100.0; // Use fixed SL for risk calc for now
             double desiredSize = calculateSize(currentPrice, accountValue, stopPips);

             if (desiredSize <= 0) return;

             Utils::logMessage("  Attempting " + std::string(entryOrderType == OrderType::BUY ? "BUY" : "SELL")
                             + " order for " + std::to_string(desiredSize) + " units. Prediction: " + std::to_string(signal.predictedValue));

             currentPendingOrderId = broker->submitOrder(entryOrderType, OrderReason::ENTRY_SIGNAL, dataName, desiredSize);
             if (currentPendingOrderId == -1) Utils::logMessage("  ERROR: Failed to submit entry order!");
             return;
        }
    }
}

// --- Stop ---
void ModelBasedStrategy::stop() {
     // Summary logic (same as RandomStrategy)
     // ...
}

// --- Notify Order ---
void ModelBasedStrategy::notifyOrder(const Order& order) {
    // Logic is very similar to RandomStrategy notifyOrder
    // BUT when an ENTRY fill occurs, calculate SL/TP using slPips/tpPips parameters

     if (order.id == currentPendingOrderId) currentPendingOrderId = -1;

     // ... (Optional debug logging) ...

     if (order.status == OrderStatus::FILLED) {
         // Entry Fill
         if (order.reason == OrderReason::ENTRY_SIGNAL && !inPosition) {
             tradeCount++;
             inPosition = true;
             // ... store position details (symbol, size, entryPrice, time, pipPoint) ...

             // Calculate SL/TP based on fixed pips
             if(useFixedSlTp) {
                 double sl_distance = slPips * pipPoint;
                 double tp_distance = tpPips * pipPoint;
                 if (currentPosition.size > 0) { // Long
                     currentPosition.stopLossPrice = order.filledPrice - sl_distance;
                     currentPosition.takeProfitPrice = order.filledPrice + tp_distance;
                 } else { // Short
                     currentPosition.stopLossPrice = order.filledPrice + sl_distance;
                     currentPosition.takeProfitPrice = order.filledPrice - tp_distance;
                 }
             } else {
                  // TODO: Set SL/TP based on signal if model provided them
                  currentPosition.stopLossPrice = 0.0; // Mark as unset
                  currentPosition.takeProfitPrice = 0.0;
             }

             Utils::logMessage(">>> ENTRY EXECUTED <<< | Trade #" + std::to_string(tradeCount) /* ... log details ... */
                            + " | SL: " + std::to_string(currentPosition.stopLossPrice)
                            + " | TP: " + std::to_string(currentPosition.takeProfitPrice));

         }
         // Exit Fill
         else if (inPosition && /* ... check position matches order ... */ )
         {
              // ... calculate PnL, increment profitableTrades, log exit ...
              inPosition = false;
              currentPosition = Position();
         }
         // ... other fill handling ...
     }
     // ... handle REJECTED/MARGIN/CANCELLED ...
}

// --- Calculate Size ---
// Adapt this from previous strategies based on positionType, fixedSize, cashPercent, riskPercent params
double ModelBasedStrategy::calculateSize(double currentPrice, double accountValue, double stopLossPips) {
    // ... implementation similar to SmaCrossStrategy/RandomStrategy using instance member parameters ...
    // Make sure to use the passed stopLossPips for risk calculation if type is 'risk'
     if (positionType == "fixed") return fixedSize > 0 ? fixedSize : 1.0;
     // ... implement percent and risk based on member variables ...
     return 1.0; // Default fallback
}