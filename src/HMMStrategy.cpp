// #include "HMMModelInterface.h"
// #include "HMMStrategy.h"
// #include "Config.h"
// #include "ModelInterface.h"
// #include "Broker.h"
// #include "Bar.h"
// #include "Order.h"
// #include "Utils.h"
// #include <vector>
// #include <string>
// #include "XGBoostModelInterface.h"

// #include <thread>
// #include <chrono>

// HMMStrategy::HMMStrategy() = default;

// std::string HMMStrategy::getName() const {
//     return "HMMStrategy";
// }

// void HMMStrategy::init() {
//     entryThreshold_ = config->getNested<double>("/Strategy/EntryThreshold", 0.0);
//     // stopLossPips_ = config->getNested<double>("/Strategy/StopLossPips", 50.0);
//     // takeProfitPips_ = config->getNested<double>("/Strategy/TakeProfitPips", 50.0);
//     pipValue_ = config->getNested<double>("/Strategy/PipValue", 1.0);
//     n_components_ = config->getNested<int>("/RegimeDetection/params/n_components", 5);

//     inPosition_ = false;
//     currentRegime_ = -1;

//     metrics = std::make_unique<TradingMetrics>(broker->getStartingCash());

//     //inni for specialsied models per regime
//     // for (int i = 0; i < n_components_; i++) {
//     //     auto model = std::make_unique<XGBoostModelInterface>();
//     //     regime_models_.push_back(std::move(model));
//     //     #ifdef _WIN32
//     //         std::wstring regimeModelPathW = Utils::UTF8ToWide("../../../xgb_saved/model_" + std::to_string(i) + ".json");
//     //     #else
//     //         std::string regimeModelPath = "../../../xgb_saved/model_" + std::to_string(i) + ".json";   
//     //     #endif
//     // }

// }
// // "1 2 3, 4 5 6, 7 8 9, 10 11 12"

// void HMMStrategy::next(const Bar& currentBar, size_t /*currentBarIndex*/, const double /*currentPrices*/) {
//     // Format the input as space-separated values in a single line
//     // This will be reshaped to a 1xN matrix on the Python side
//     std::string input = std::to_string(currentBar.columns[0]);
    
//     // Add the rest of the features
//     for (int i = 1; i < 5; i++) {
//         input += " " + std::to_string(currentBar.columns[i]);
//     }

//     Utils::logMessage("Sending features to HMM: " + input);
//     int regime = HMMModelInterface::use(input);

//     handlePrediction((float) regime);
// }

// void HMMStrategy::stop() {
//     Utils::logMessage("HMMStrategy: Backtest finished.");
// }

// void HMMStrategy::notifyOrder(const Order& /*order*/) {
//     // TODO: order notification handling
// }

// void HMMStrategy::handlePrediction(float prediction){
//     Utils::logMessage("Current Regime:" + std::to_string(prediction));
// }



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

void HMMStrategy::next(const Bar& currentBar, size_t /*currentBarIndex*/, const double /*currentPrices*/) {
    // Prepare feature vector: using closing price and any extra features
    std::vector<float> features;
    features.push_back(static_cast<float>(currentBar.columns[0]));
    std::vector<int64_t> shape = {1, static_cast<int64_t>(features.size())};

    std::vector<float> reg_out;

    reg_out = hmm_model_->Predict(features, shape);

    if (!reg_out.empty()) {
        currentRegime_ = static_cast<int>(reg_out[0]);
    }

    handlePrediction((float) currentRegime_);
}

void HMMStrategy::stop() {
    Utils::logMessage("HMMStrategy: Backtest finished.");
}

void HMMStrategy::notifyOrder(const Order& /*order*/) {
    // TODO: order notification handling
}

void HMMStrategy::handlePrediction(float prediction){
    Utils::logMessage("Current Regime:" + std::to_string(prediction));
}

// // MLStrategy::MLStrategy()
// //     : inPosition_(false), entryPrice_(0.0), currentRegime_(-1) {}

// // void MLStrategy::init() {
// //     // Load strategy parameters from config
// //     entryThreshold_ = config->getNested<double>("/Strategy/EntryThreshold", 0.0);
// //     stopLossPips_ = config->getNested<double>("/Strategy/StopLossPips", 50.0);
// //     takeProfitPips_ = config->getNested<double>("/Strategy/TakeProfitPips", 50.0);
// //     // pip value based on symbol (stubbed as 1.0)
// //     pipValue_ = 1.0;

// //     // Load HMM ONNX model
// //     std::string hmmPath = config->getNested<std::string>("/Strategy/HMMOnnxPath", "");
// //     hmm_model_ = std::make_unique<OnnxModelInterface>();
// // #ifdef _WIN32
// //     {
// //         std::wstring hmmPathW(hmmPath.begin(), hmmPath.end());
// //         if (!hmm_model_->LoadModel(hmmPathW.c_str())) {
// //             Utils::logMessage("MLStrategy Error: Failed to load HMM model from " + hmmPath);
// //         }
// //     }
// // #else
// //     if (!hmm_model_->LoadModel(hmmPath.c_str())) {
// //         Utils::logMessage("MLStrategy Error: Failed to load HMM model from " + hmmPath);
// //     }
// // #endif

// //     // Load per-regime models
// //     // Expect a JSON object mapping regime to model path
// //     auto rm = config->getNested<nlohmann::json>("/Strategy/RegimeModelOnnxPaths", nlohmann::json::object());
// // //     for (auto& kv : rm.items()) {
// // //         std::string path = kv.value();
// // // #ifdef _WIN32
// // //         std::wstring pathW(path.begin(), path.end());
// // //         auto m = new ;
// // // #else
// // //         auto m = ModelInterface::CreateModel(path.c_str());
// // // #endif
// // //         if (m) regime_models_.push_back(std::move(m));
// // //         else Utils::logMessage("MLStrategy Error: Failed to load model for regime " + kv.key());
// // //     }
// // }

// // void MLStrategy::next(const Bar& /*currentBar*/, size_t /*currentBarIndex*/, const std::map<std::string, double>& currentPrices) {
// //     // Build feature vector: use currentPrices values
// //     std::vector<float> features;
// //     for (auto& kv : currentPrices) features.push_back(static_cast<float>(kv.second));
// //     std::vector<int64_t> shape = {1, static_cast<int64_t>(features.size())};

// //     // Predict regime
// //     auto reg_out = hmm_model_->Predict(features, shape);
// //     if (!reg_out.empty()) currentRegime_ = static_cast<int>(reg_out[0]);
    
// //     // Predict value/signal
// //     float pred = 0.0f;
// //     if (currentRegime_ >= 0 && currentRegime_ < (int)regime_models_.size()) {
// //         pred = regime_models_[currentRegime_]->Predict(features, shape)[0];
// //     }

// //     double price = currentPrices.at(dataName); // assume dataName symbol key
    
// //     if (!inPosition_) {
// //         handlePrediction(pred, price);
// //     }
// // }

// // void MLStrategy::stop() {
// //     Utils::logMessage("MLStrategy: Backtest finished.");
// // }

// // void MLStrategy::notifyOrder(const Order& order) {
// //     if (order.status == OrderStatus::FILLED) {
// //         if (order.type == OrderType::BUY) {
// //             inPosition_ = true;
// //             entryPrice_ = order.filledPrice;
// //         } else if (order.type == OrderType::SELL) {
// //             inPosition_ = false;
// //         }
// //     }
// // }

// // // Default handlePrediction implementation
// // void MLStrategy::handlePrediction(float prediction, double price) {
// //     if (prediction > entryThreshold_) {
// //         Order o;
// //         o.symbol = dataName;
// //         o.type = OrderType::BUY;
// //         o.requestedSize = 1.0;
// //         o.takeProfit = price + takeProfitPips_ * pipValue_;
// //         o.stopLoss = price - stopLossPips_ * pipValue_;
// //         broker->submitOrder(o);
// //     }
// // }

// // // Example override
// // void ExampleMLStrategy::handlePrediction(float prediction, double price) {
// //     if (prediction > entryThreshold_) {
// //         Order o;
// //         o.symbol = dataName;
// //         o.type = OrderType::BUY;
// //         o.requestedSize = 1.0;
// //         o.takeProfit = price + takeProfitPips_ * pipValue_;
// //         o.stopLoss = price - stopLossPips_ * pipValue_;
// //         broker->submitOrder(o);
// //     } else if (prediction < -entryThreshold_) {
// //         Order o;
// //         o.symbol = dataName;
// //         o.type = OrderType::SELL;
// //         o.requestedSize = 1.0;
// //         o.takeProfit = price - takeProfitPips_ * pipValue_;
// //         o.stopLoss = price + stopLossPips_ * pipValue_;
// //         broker->submitOrder(o);
// //     }
// // }