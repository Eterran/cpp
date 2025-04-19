#include "HMMStrategy.h"
#include "Config.h"
#include "ModelInterface.h"
#include "Broker.h"
#include "Bar.h"
#include "Order.h"
#include "Utils.h"
#include "HMMModelInterface.h"
#include <vector>
#include <string>
#include "XGBoostModelInterface.h"

#include <Python.h>
#include <thread>
#include <chrono>

HMMStrategy::HMMStrategy() 
    : inPosition_(false), 
      entryPrice_(0.0), 
      currentRegime_(-1),
      previousRegime_(-1),
      positionType_(0),
      lastPrediction_(0.0),
      trailStopPrice_(0.0) {}

std::string HMMStrategy::getName() const {
    return "HMMStrategy";
}

void HMMStrategy::init() {
    // Load strategy parameters
    entryThreshold_ = config->getNested<double>("/Strategy/EntryThreshold", 0.0);
    // stopLossPips_ = config->getNested<double>("/Strategy/StopLossPips", 50.0);
    // takeProfitPips_ = config->getNested<double>("/Strategy/TakeProfitPips", 50.0);
    pipValue_ = config->getNested<double>("/Strategy/PipValue", 1.0);
    n_components_ = config->getNested<int>("/RegimeDetection/params/n_components", 5);

    // Load HMM regime detection model
    std::string hmmPath = config->getNested<std::string>("/RegimeDetection/model_path", "");
    hmm_model_ = std::make_unique<HMMModelInterface>();
    #ifdef _WIN32
    std::wstring hmmPathW = Utils::UTF8ToWide(hmmPath);
        if (!hmm_model_->LoadModel(hmmPathW.c_str())) {
            Utils::logMessage("HMMStrategy Error: Failed to load HMM model from " + hmmPath);
        }
    #else
        if (!hmm_model_->LoadModel(hmmPath.c_str())) {
            Utils::logMessage("HMMStrategy Error: Failed to load HMM model from " + hmmPath);
        }
    #endif
    inPosition_ = false;
    currentRegime_ = -1;

    metrics = std::make_unique<TradingMetrics>(broker->getStartingCash());

    //inni for specialsied models per regime
    for (int i = 0; i < n_components_; i++) {
        auto model = std::make_unique<XGBoostModelInterface>();
        #ifdef _WIN32
            std::wstring regimeModelPathW = Utils::UTF8ToWide("../../../xgb_saved/model_" + std::to_string(i) + ".json");
            if (!model->LoadModel(regimeModelPathW.c_str())) {
                Utils::logMessage("HMMStrategy Error: Failed to load XGBoost model for regime " + std::to_string(i));
            } else {
                Utils::logMessage("Successfully loaded XGBoost model for regime " + std::to_string(i));
            }
        #else
            std::string regimeModelPath = "../../../xgb_saved/model_" + std::to_string(i) + ".json";
            if (!model->LoadModel(regimeModelPath.c_str())) {
                Utils::logMessage("HMMStrategy Error: Failed to load XGBoost model for regime " + std::to_string(i));
            } else {
                Utils::logMessage("Successfully loaded XGBoost model for regime " + std::to_string(i));
            }
        #endif
        regime_models_.push_back(std::move(model));
    }


    // Py_Initialize();
    // const char* py_ver = Py_GetVersion();
    // std::cout << "Python version: " << py_ver << std::endl;
    // std::cout << (std::string) pybind11::str(pybind11::module::import("sys").attr("executable")) << std::endl;
    // Py_Finalize();
    // std::this_thread::sleep_for(std::chrono::seconds(10));
}

void HMMStrategy::next(const Bar& currentBar, size_t currentBarIndex, const double /*currentPrices*/) {
    // Store current bar in global history
    allBarHistory_.push_back(currentBar);
    
    const size_t MIN_HISTORY = 30;
    if (allBarHistory_.size() < MIN_HISTORY) {
        Utils::logMessage("HMMStrategy: Not enough bars for prediction yet, have " + 
                         std::to_string(allBarHistory_.size()) + ", need " + 
                         std::to_string(MIN_HISTORY));
        return;
    }
    
    // The window size needs to be large enough to capture the regime transitions
    const size_t WINDOW_SIZE = 100;
    size_t startIdx = allBarHistory_.size() <= WINDOW_SIZE ? 0 : allBarHistory_.size() - WINDOW_SIZE;
    
    // Extract features from relevant history with a larger window
    std::vector<std::vector<float>> rawFeatures;
    for (size_t i = startIdx; i < allBarHistory_.size(); i++) {
        std::vector<float> features;
        for (int j = 0; j < 4; j++) {
            features.push_back(static_cast<float>(allBarHistory_[i].columns[j]));
        }
        rawFeatures.push_back(features);
    }
    
    // Apply z-score normalization (subtract mean, divide by std dev)
    auto normalizedFeatures = normalizeFeatures(rawFeatures);
    
    // Predict regimes for all samples in the window
    std::vector<float> regimePredictions = hmm_model_->predict2D(normalizedFeatures);
    
    // Get my current regime (the last prediction)
    if (!regimePredictions.empty()) {
        currentRegime_ = static_cast<int>(regimePredictions.back());
        
        // Log the distribution of regimes in this window
        std::map<int, int> regimeCounts;
        for (float regime : regimePredictions) {
            regimeCounts[static_cast<int>(regime)]++;
        }
        
        std::string distribution = "Regime distribution: ";
        for (const auto& [regime, count] : regimeCounts) {
            distribution += "Regime " + std::to_string(regime) + ": " + 
                           std::to_string(count) + " (" +
                           std::to_string(static_cast<float>(count) / regimePredictions.size() * 100.0f) +
                           "%), ";
        }
        Utils::logMessage(distribution);
        
        // Every N bars, log the full regime sequence for debugging
        const int LOG_FULL_SEQUENCE_INTERVAL = 50;
        if (currentBarIndex % LOG_FULL_SEQUENCE_INTERVAL == 0) {
            std::string regimeSequence = "Full regime sequence: ";
            for (size_t i = 0; i < std::min(regimePredictions.size(), static_cast<size_t>(20)); i++) {
                regimeSequence += std::to_string(static_cast<int>(regimePredictions[i])) + " ";
            }
            if (regimePredictions.size() > 20) {
                regimeSequence += "... ";
                // Also show the last few predictions
                for (size_t i = regimePredictions.size() - 5; i < regimePredictions.size(); i++) {
                    regimeSequence += std::to_string(static_cast<int>(regimePredictions[i])) + " ";
                }
            }
            Utils::logMessage(regimeSequence);
        }
        
        handlePrediction(currentRegime_);
    } else {
        Utils::logMessage("Error: Empty prediction from HMM model");
    }
}

// Normalize features using z-score normalization (subtract mean, divide by std)
std::vector<std::vector<float>> HMMStrategy::normalizeFeatures(const std::vector<std::vector<float>>& features) {
    if (features.empty() || features[0].empty()) return features;
    
    const size_t numSamples = features.size();
    const size_t numFeatures = features[0].size();
    
    // Calculate means for each feature
    std::vector<float> means(numFeatures, 0.0f);
    for (const auto& sample : features) {
        for (size_t j = 0; j < numFeatures; j++) {
            means[j] += sample[j];
        }
    }
    for (size_t j = 0; j < numFeatures; j++) {
        means[j] /= numSamples;
    }
    
    // Calculate standard deviations for each feature
    std::vector<float> stds(numFeatures, 0.0f);
    for (const auto& sample : features) {
        for (size_t j = 0; j < numFeatures; j++) {
            float diff = sample[j] - means[j];
            stds[j] += diff * diff;
        }
    }
    for (size_t j = 0; j < numFeatures; j++) {
        stds[j] = std::sqrt(stds[j] / numSamples);
        // Avoid division by zero
        if (stds[j] < 1e-6) stds[j] = 1.0f;
    }
    
    // Log normalization parameters
    // std::string meanStr = "Feature means: ";
    // std::string stdStr = "Feature std devs: ";
    // for (size_t j = 0; j < numFeatures; j++) {
    //     meanStr += std::to_string(means[j]) + " ";
    //     stdStr += std::to_string(stds[j]) + " ";
    // }
    // Utils::logMessage(meanStr);
    // Utils::logMessage(stdStr);
    
    // Apply z-score normalization
    std::vector<std::vector<float>> normalized(numSamples, std::vector<float>(numFeatures));
    for (size_t i = 0; i < numSamples; i++) {
        for (size_t j = 0; j < numFeatures; j++) {
            normalized[i][j] = (features[i][j] - means[j]) / stds[j];
        }
    }
    
    return normalized;
}

void HMMStrategy::stop() {
    Utils::logMessage("HMMStrategy: Backtest finished.");
}

void HMMStrategy::notifyOrder(const Order& order) {
    if (order.status == OrderStatus::FILLED) {
        if (order.type == OrderType::BUY) {
            inPosition_ = true;
            entryPrice_ = order.filledPrice;
            positionType_ = 1; // Long position
            Utils::logMessage("Long position entered at " + std::to_string(entryPrice_));
            metrics->recordTrade(false); // Entry trade - not yet profitable
        } else if (order.type == OrderType::SELL) {
            if (inPosition_ && positionType_ == 1) {
                // Closing a long position
                inPosition_ = false;
                double pnl = order.filledPrice - entryPrice_;
                bool profitable = (pnl > 0);
                Utils::logMessage("Closed long position: P&L = " + std::to_string(pnl));
                metrics->recordTrade(profitable);
            } else {
                // Opening a short position
                inPosition_ = true;
                entryPrice_ = order.filledPrice;
                positionType_ = -1; // Short position
                Utils::logMessage("Short position entered at " + std::to_string(entryPrice_));
                metrics->recordTrade(false); // Entry trade - not yet profitable
            }
        }
    }
}

void HMMStrategy::handlePrediction(int regime){
    Utils::logMessage("Current Regime: " + std::to_string(regime));
    
    // Check if regime is valid
    if (regime < 0 || regime >= static_cast<int>(regime_models_.size())) {
        Utils::logMessage("Invalid regime: " + std::to_string(regime));
        return;
    }
    
    // Get the model for the current regime
    auto& model = regime_models_[regime];
    
    // Extract features from the most recent bar (if available)
    if (allBarHistory_.empty()) {
        Utils::logMessage("No bar history available for prediction");
        return;
    }
    
    // Get the most recent bar
    const Bar& currentBar = allBarHistory_.back();
    
    // Extract features from current bar
    std::vector<float> features;
    for (int i = 0; i < 4; i++) {
        features.push_back(static_cast<float>(currentBar.columns[i]));
    }
    
    // Use the shape for prediction
    std::vector<int64_t> shape = {1, static_cast<int64_t>(features.size())};
    
    // Make prediction using the regime-specific model
    std::vector<float> prediction = model->Predict(features, shape);
    
    if (prediction.empty()) {
        Utils::logMessage("Empty prediction from regime " + std::to_string(regime) + " model");
        return;
    }
    
    float predValue = prediction[0];
    Utils::logMessage("Regime " + std::to_string(regime) + " model prediction: " + std::to_string(predValue));
    
    // Store the current prediction for future reference
    lastPrediction_ = predValue;
    
    // Trading logic based on prediction
    double currentPrice = currentBar.columns[3]; // Assuming column 3 is the close price
    
    // First, check if we should exit an existing position
    if (inPosition_) {
        // Update the trailing stop with the current price
        updateTrailStop(currentPrice);
        
        // Check exit conditions
        if (shouldExitPosition(regime, predValue, currentPrice)) {
            // Exit the position
            Order exitOrder;
            exitOrder.symbol = dataName;
            
            if (positionType_ == 1) { // Long position
                exitOrder.type = OrderType::SELL;
                Utils::logMessage("Exiting LONG position at " + std::to_string(currentPrice));
            } else { // Short position
                exitOrder.type = OrderType::BUY;
                Utils::logMessage("Exiting SHORT position at " + std::to_string(currentPrice));
            }
            
            exitOrder.requestedSize = 1.0; // Same size as entry
            broker->submitOrder(exitOrder);
            
            // Reset trailing stop
            trailStopPrice_ = 0.0;
        }
    } else {
        // Not in a position, check if we should enter one
        if (predValue > entryThreshold_) {
            // Bullish signal - go long
            double positionSize = calculatePositionSize(predValue, regime, currentPrice);
            
            Order o;
            o.symbol = dataName;
            o.type = OrderType::BUY;
            o.requestedSize = positionSize;
            
            Utils::logMessage("Regime " + std::to_string(regime) + " - Submitting BUY order at " + std::to_string(currentPrice) + 
                             " with size " + std::to_string(positionSize));
            broker->submitOrder(o);
            
            // Initialize trail stop
            trailStopPrice_ = currentPrice * 0.995; // Initial 0.5% trail stop
        } 
        else if (predValue < -entryThreshold_) {
            // Bearish signal - go short
            double positionSize = calculatePositionSize(-predValue, regime, currentPrice);
            
            Order o;
            o.symbol = dataName;
            o.type = OrderType::SELL;
            o.requestedSize = positionSize;
            
            Utils::logMessage("Regime " + std::to_string(regime) + " - Submitting SELL order at " + std::to_string(currentPrice) +
                             " with size " + std::to_string(positionSize));
            broker->submitOrder(o);
            
            // Initialize trail stop
            trailStopPrice_ = currentPrice * 1.005; // Initial 0.5% trail stop
        }
    }
    
    // Store current regime as previous for next iteration
    previousRegime_ = regime;
}

double HMMStrategy::calculateRegimeVolatility(int regime, size_t lookback) {
    // Calculate the volatility for a specific regime using historical data
    if (allBarHistory_.size() < lookback) {
        return 0.01; // Default volatility if not enough history
    }
    
    // Find bars where this regime was active
    std::vector<double> prices;
    size_t startIdx = allBarHistory_.size() - std::min(lookback, allBarHistory_.size());
    
    for (size_t i = startIdx; i < allBarHistory_.size(); i++) {
        prices.push_back(allBarHistory_[i].columns[3]); // Assuming close price is at index 3
    }
    
    // Calculate standard deviation of returns
    if (prices.size() < 2) return 0.01;
    
    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); i++) {
        returns.push_back((prices[i] / prices[i-1]) - 1.0);
    }
    
    // Calculate mean
    double sum = 0.0;
    for (double ret : returns) {
        sum += ret;
    }
    double mean = sum / returns.size();
    
    // Calculate standard deviation
    double sumSquaredDiff = 0.0;
    for (double ret : returns) {
        double diff = ret - mean;
        sumSquaredDiff += diff * diff;
    }
    
    double volatility = std::sqrt(sumSquaredDiff / returns.size());
    
    // Cache the result
    regimeVolatility_[regime] = volatility;
    
    Utils::logMessage("Calculated volatility for regime " + std::to_string(regime) + ": " + std::to_string(volatility));
    return volatility;
}

double HMMStrategy::calculatePositionSize(float predictionStrength, int regime, double price) {
    // Get or calculate the volatility for this regime
    double volatility = 0.01; // Default
    auto it = regimeVolatility_.find(regime);
    if (it != regimeVolatility_.end()) {
        volatility = it->second;
    } else {
        volatility = calculateRegimeVolatility(regime);
    }
    
    // Base position size (adjust these parameters as needed)
    double maxRiskPercent = 0.02; // 2% risk per trade
    double accountSize = broker->getCash();
    
    // Adjust size based on prediction strength (higher conviction = larger size)
    double predictionFactor = std::min(1.0, std::abs(predictionStrength) / 2.0);
    
    // Adjust size inversely to volatility (higher volatility = smaller size)
    double volatilityFactor = std::min(1.0, 0.01 / volatility);
    
    // Calculate final size
    double positionValue = accountSize * maxRiskPercent * predictionFactor * volatilityFactor;
    double positionSize = positionValue / price;
    
    // Round to 2 decimal places
    positionSize = std::round(positionSize * 100.0) / 100.0;
    
    // Ensure minimum size
    double minSize = 0.01;
    if (positionSize < minSize) positionSize = minSize;
    
    Utils::logMessage("Calculated position size: " + std::to_string(positionSize) + " based on prediction: " + 
                     std::to_string(predictionStrength) + " and regime volatility: " + std::to_string(volatility));
    
    return positionSize;
}

void HMMStrategy::updateTrailStop(double currentPrice) {
    if (!inPosition_) return;
    
    // For long positions
    if (positionType_ == 1) {
        // If current price is higher than previous trail stop, update it
        double newTrailStop = currentPrice * 0.995; // 0.5% trailing stop
        if (trailStopPrice_ == 0 || newTrailStop > trailStopPrice_) {
            trailStopPrice_ = newTrailStop;
            Utils::logMessage("Updated trail stop for LONG position to: " + std::to_string(trailStopPrice_));
        }
    } 
    // For short positions
    else if (positionType_ == -1) {
        // If current price is lower than previous trail stop, update it
        double newTrailStop = currentPrice * 1.005; // 0.5% trailing stop
        if (trailStopPrice_ == 0 || newTrailStop < trailStopPrice_) {
            trailStopPrice_ = newTrailStop;
            Utils::logMessage("Updated trail stop for SHORT position to: " + std::to_string(trailStopPrice_));
        }
    }
}

bool HMMStrategy::shouldExitPosition(int regime, float prediction, double currentPrice) {
    if (!inPosition_) return false;
    
    // Check for regime change
    if (previousRegime_ != -1 && regime != previousRegime_) {
        Utils::logMessage("Exiting position due to regime change from " + 
                         std::to_string(previousRegime_) + " to " + std::to_string(regime));
        return true;
    }
    
    // Check for prediction reversal
    if (positionType_ == 1 && prediction < -entryThreshold_) {
        Utils::logMessage("Exiting LONG position due to bearish prediction: " + std::to_string(prediction));
        return true;
    } else if (positionType_ == -1 && prediction > entryThreshold_) {
        Utils::logMessage("Exiting SHORT position due to bullish prediction: " + std::to_string(prediction));
        return true;
    }
    
    // Check trail stop
    if (trailStopPrice_ != 0) {
        if (positionType_ == 1 && currentPrice < trailStopPrice_) {
            Utils::logMessage("Exiting LONG position due to trail stop hit: current=" + 
                             std::to_string(currentPrice) + ", stop=" + std::to_string(trailStopPrice_));
            return true;
        } else if (positionType_ == -1 && currentPrice > trailStopPrice_) {
            Utils::logMessage("Exiting SHORT position due to trail stop hit: current=" + 
                             std::to_string(currentPrice) + ", stop=" + std::to_string(trailStopPrice_));
            return true;
        }
    }
    
    return false;
}