// #include "MLStrategy.h"
// #include "Config.h"
// #include "ModelInterface.h"
// #include "Broker.h"
// #include "Bar.h"
// #include "Order.h"
// #include "Utils.h"
// #include <vector>
// #include <string>
// #include "OnnxModelInterface.h"

// MLStrategy::MLStrategy()
//     : inPosition_(false), entryPrice_(0.0), currentRegime_(-1) {}

// void MLStrategy::init() {
//     // Load strategy parameters from config
//     entryThreshold_ = config->getNested<double>("/Strategy/EntryThreshold", 0.0);
//     stopLossPips_ = config->getNested<double>("/Strategy/StopLossPips", 50.0);
//     takeProfitPips_ = config->getNested<double>("/Strategy/TakeProfitPips", 50.0);
//     // pip value based on symbol (stubbed as 1.0)
//     pipValue_ = 1.0;

//     // Load HMM ONNX model
//     std::string hmmPath = config->getNested<std::string>("/Strategy/HMMOnnxPath", "");
//     hmm_model_ = std::make_unique<OnnxModelInterface>();
// #ifdef _WIN32
//     {
//         std::wstring hmmPathW(hmmPath.begin(), hmmPath.end());
//         if (!hmm_model_->LoadModel(hmmPathW.c_str())) {
//             Utils::logMessage("MLStrategy Error: Failed to load HMM model from " + hmmPath);
//         }
//     }
// #else
//     if (!hmm_model_->LoadModel(hmmPath.c_str())) {
//         Utils::logMessage("MLStrategy Error: Failed to load HMM model from " + hmmPath);
//     }
// #endif

//     // Load per-regime models
//     // Expect a JSON object mapping regime to model path
//     auto rm = config->getNested<nlohmann::json>("/Strategy/RegimeModelOnnxPaths", nlohmann::json::object());
// //     for (auto& kv : rm.items()) {
// //         std::string path = kv.value();
// // #ifdef _WIN32
// //         std::wstring pathW(path.begin(), path.end());
// //         auto m = new ;
// // #else
// //         auto m = ModelInterface::CreateModel(path.c_str());
// // #endif
// //         if (m) regime_models_.push_back(std::move(m));
// //         else Utils::logMessage("MLStrategy Error: Failed to load model for regime " + kv.key());
// //     }
// }

// void MLStrategy::next(const Bar& /*currentBar*/, size_t /*currentBarIndex*/, const std::map<std::string, double>& currentPrices) {
//     // Build feature vector: use currentPrices values
//     std::vector<float> features;
//     for (auto& kv : currentPrices) features.push_back(static_cast<float>(kv.second));
//     std::vector<int64_t> shape = {1, static_cast<int64_t>(features.size())};

//     // Predict regime
//     auto reg_out = hmm_model_->Predict(features, shape);
//     if (!reg_out.empty()) currentRegime_ = static_cast<int>(reg_out[0]);
    
//     // Predict value/signal
//     float pred = 0.0f;
//     if (currentRegime_ >= 0 && currentRegime_ < (int)regime_models_.size()) {
//         pred = regime_models_[currentRegime_]->Predict(features, shape)[0];
//     }

//     double price = currentPrices.at(dataName); // assume dataName symbol key
    
//     if (!inPosition_) {
//         handlePrediction(pred, price);
//     }
// }

// void MLStrategy::stop() {
//     Utils::logMessage("MLStrategy: Backtest finished.");
// }

// void MLStrategy::notifyOrder(const Order& order) {
//     if (order.status == OrderStatus::FILLED) {
//         if (order.type == OrderType::BUY) {
//             inPosition_ = true;
//             entryPrice_ = order.filledPrice;
//         } else if (order.type == OrderType::SELL) {
//             inPosition_ = false;
//         }
//     }
// }

// // Default handlePrediction implementation
// void MLStrategy::handlePrediction(float prediction, double price) {
//     if (prediction > entryThreshold_) {
//         Order o;
//         o.symbol = dataName;
//         o.type = OrderType::BUY;
//         o.requestedSize = 1.0;
//         o.takeProfit = price + takeProfitPips_ * pipValue_;
//         o.stopLoss = price - stopLossPips_ * pipValue_;
//         broker->submitOrder(o);
//     }
// }

// // Example override
// void ExampleMLStrategy::handlePrediction(float prediction, double price) {
//     if (prediction > entryThreshold_) {
//         Order o;
//         o.symbol = dataName;
//         o.type = OrderType::BUY;
//         o.requestedSize = 1.0;
//         o.takeProfit = price + takeProfitPips_ * pipValue_;
//         o.stopLoss = price - stopLossPips_ * pipValue_;
//         broker->submitOrder(o);
//     } else if (prediction < -entryThreshold_) {
//         Order o;
//         o.symbol = dataName;
//         o.type = OrderType::SELL;
//         o.requestedSize = 1.0;
//         o.takeProfit = price - takeProfitPips_ * pipValue_;
//         o.stopLoss = price + stopLossPips_ * pipValue_;
//         broker->submitOrder(o);
//     }
// }