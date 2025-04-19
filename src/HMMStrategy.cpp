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

HMMStrategy::HMMStrategy() = default;

std::string HMMStrategy::getName() const {
    return "HMMStrategy";
}

void HMMStrategy::init() {
    // Load strategy parameters
    entryThreshold_ = config->getNested<double>("/Strategy/EntryThreshold", 0.0);
    // stopLossPips_ = config->getNested<double>("/Strategy/StopLossPips", 50.0);
    // takeProfitPips_ = config->getNested<double>("/Strategy/TakeProfitPips", 50.0);
    pipValue_ = config->getNested<double>("/Strategy/PipValue", 1.0);
    n_components_ = config->getNested<int>("/RegimeDetection/params/n_components", 3);

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
        regime_models_.push_back(std::move(model));
        #ifdef _WIN32
            std::wstring regimeModelPathW = Utils::UTF8ToWide("../../../xgb_saved/model_" + std::to_string(i) + ".json");
        #else
            std::string regimeModelPath = "../../../xgb_saved/model_" + std::to_string(i) + ".json";   
        #endif
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
        
        handlePrediction(static_cast<float>(currentRegime_));
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

void HMMStrategy::notifyOrder(const Order& /*order*/) {
    // TODO: order notification handling
}

void HMMStrategy::handlePrediction(int regime){
    Utils::logMessage("Current Regime:" + std::to_string(regime));
}