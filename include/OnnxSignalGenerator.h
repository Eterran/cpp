// include/OnnxSignalGenerator.h
#ifndef ONNXSIGNALGENERATOR_H
#define ONNXSIGNALGENERATOR_H

#include "ISignalGenerator.h"
#include <vector>
#include <string>
#include <memory>
#include <onnxruntime_cxx_api.h> // ONNX Runtime Header

class OnnxSignalGenerator : public ISignalGenerator {
private:
    // ONNX Runtime Objects
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;

    // Model Info
    std::vector<const char*> input_node_names; // C-style strings needed by API
    std::vector<const char*> output_node_names;
    std::vector<std::string> input_node_names_str; // Store std::string for ownership
    std::vector<std::string> output_node_names_str;
    std::vector<int64_t> input_node_dims; // Shape of input tensor e.g. {1, windowSize * features}

    // Parameters from Config
    int windowSize = 60; // Example default
    std::vector<std::string> featureCols = {"close"}; // Example: just use close price

    // Data Pointer
    const std::vector<Bar>* historyData = nullptr;

    // Input Buffer
    std::vector<float> inputTensorValues; // Flattened feature data

public:
    // Constructor sets up environment and session options
    OnnxSignalGenerator();
    ~OnnxSignalGenerator() override = default; // Use default destructor

    // Load model, get parameters, store data pointer
    bool init(const Config* config, const std::vector<Bar>* historyData) override;

    // Prepare features, run inference, process output
    TradingSignal generateSignal(size_t currentBarIndex) override;
};

#endif // ONNXSIGNALGENERATOR_H