// src/OnnxSignalGenerator.cpp
#include "OnnxSignalGenerator.h"
#include "Utils.h" // For logging
#include <stdexcept>
#include <numeric> // For std::accumulate, iota
#include <algorithm> // For std::transform

// Constructor: Initialize ONNX Env and SessionOptions
OnnxSignalGenerator::OnnxSignalGenerator() :
    env(ORT_LOGGING_LEVEL_WARNING, "ONNXSignalGen") // Create ONNX environment
{
    // Configure Session Options (e.g., parallelism, providers)
    session_options.SetIntraOpNumThreads(1); // Example: Use 1 thread
    // session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED); // Example
    // Add execution providers if needed (e.g., CUDA, DirectML)
}

// Initialize: Load Model, Parameters
bool OnnxSignalGenerator::init(const Config* config, const std::vector<Bar>* historyDataPtr) {
    if (!config || !historyDataPtr) {
        Utils::logMessage("OnnxSignalGenerator Error: Config or HistoryData pointer is null.");
        return false;
    }
    this->historyData = historyDataPtr; // Store pointer

    try {
        // --- Load Parameters from Config ---
        std::string modelPath = config->getNested<std::string>("/Strategy/ONNX/MODEL_PATH", "");
        windowSize = config->getNested<int>("/Strategy/ONNX/WINDOW_SIZE", 60);
        // Feature columns could be a JSON array in config: ["open", "high", "low", "close", "volume"]
        featureCols = config->getNested<std::vector<std::string>>("/Strategy/ONNX/FEATURE_COLS", {"close"}); // Default to close only

        // Input/Output Names (should match your exported ONNX model)
        // These could also be loaded as arrays from config
        input_node_names_str = config->getNested<std::vector<std::string>>("/Strategy/ONNX/INPUT_NAMES", {"input_tensor"}); // Example name
        output_node_names_str = config->getNested<std::vector<std::string>>("/Strategy/ONNX/OUTPUT_NAMES", {"output_tensor"}); // Example name


        if (modelPath.empty()) throw std::runtime_error("ONNX/MODEL_PATH not specified in config.");
        if (windowSize <= 0) throw std::runtime_error("ONNX/WINDOW_SIZE must be positive.");
        if (featureCols.empty()) throw std::runtime_error("ONNX/FEATURE_COLS cannot be empty.");
        if (input_node_names_str.empty()) throw std::runtime_error("ONNX/INPUT_NAMES cannot be empty.");
        if (output_node_names_str.empty()) throw std::runtime_error("ONNX/OUTPUT_NAMES cannot be empty.");


        // --- Load ONNX Model ---
        Utils::logMessage("OnnxSignalGenerator: Loading model from: " + modelPath);
        #ifdef _WIN32
            // Windows requires wchar_t path
            std::wstring modelPathW(modelPath.begin(), modelPath.end());
            session = std::make_unique<Ort::Session>(env, modelPathW.c_str(), session_options);
        #else
            session = std::make_unique<Ort::Session>(env, modelPath.c_str(), session_options);
        #endif
        Utils::logMessage("OnnxSignalGenerator: Model loaded successfully.");

        // --- Prepare C-style char* vectors for API ---
        input_node_names.resize(input_node_names_str.size());
        std::transform(input_node_names_str.begin(), input_node_names_str.end(), input_node_names.begin(),
                       [](const std::string& s){ return s.c_str(); });

        output_node_names.resize(output_node_names_str.size());
        std::transform(output_node_names_str.begin(), output_node_names_str.end(), output_node_names.begin(),
                       [](const std::string& s){ return s.c_str(); });


        // --- Determine Input Shape & Preallocate Buffer ---
        // Assuming a flattened input shape: [1, windowSize * numFeatures]
        // TODO: Allow configuration or detection of shape from model inspection
        int numFeatures = featureCols.size();
        input_node_dims = {1, static_cast<int64_t>(windowSize * numFeatures)}; // Batch size 1
        inputTensorValues.resize(windowSize * numFeatures);

         Utils::logMessage("OnnxSignalGenerator Init: Window=" + std::to_string(windowSize)
                           + ", Features=" + std::to_string(numFeatures)
                           + ", InputShape=[1, " + std::to_string(windowSize * numFeatures) + "]");


    } catch (const Ort::Exception& e) {
        Utils::logMessage("OnnxSignalGenerator Ort::Exception during init: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        Utils::logMessage("OnnxSignalGenerator std::exception during init: " + std::string(e.what()));
        return false;
    }
    return true;
}

// Generate Signal: Prepare Features, Run Inference
TradingSignal OnnxSignalGenerator::generateSignal(size_t currentBarIndex) {
    TradingSignal signal; // Default action is HOLD
    signal.predictedValue = 0.0; // Default prediction

    // --- Check History Size ---
    if (currentBarIndex < static_cast<size_t>(windowSize - 1) || !historyData || historyData->empty()) {
        // Not enough data yet for a full window
        return signal;
    }

    // --- Feature Engineering ---
    try {
        int featureCount = featureCols.size();
        size_t bufferIdx = 0;
        size_t startIdx = currentBarIndex - windowSize + 1;

        for (size_t i = 0; i < static_cast<size_t>(windowSize); ++i) {
            const Bar& bar = historyData->at(startIdx + i);
            for (const std::string& col : featureCols) {
                 // Extract feature based on column name
                 // Convert to float for the model
                 float featureValue = 0.0f;
                 if (col == "open") featureValue = static_cast<float>(bar.open);
                 else if (col == "high") featureValue = static_cast<float>(bar.high);
                 else if (col == "low") featureValue = static_cast<float>(bar.low);
                 else if (col == "close") featureValue = static_cast<float>(bar.close);
                 else if (col == "volume") featureValue = static_cast<float>(bar.volume);
                 else if (col == "bid") featureValue = static_cast<float>(bar.bid);
                 else if (col == "ask") featureValue = static_cast<float>(bar.ask);
                 else if (col == "mid") featureValue = static_cast<float>(bar.midPrice());
                 // Add more features: indicators, time features etc.
                 else {
                      // Log warning for unknown feature? Only once?
                      // static bool warned = false; if (!warned) { Utils::logMessage("Warning: Unknown feature column: " + col); warned = true; }
                 }

                 // TODO: Apply scaling/normalization if required by the model
                 // featureValue = (featureValue - mean) / stddev;

                if (bufferIdx < inputTensorValues.size()) {
                    inputTensorValues[bufferIdx++] = featureValue;
                } else {
                     throw std::runtime_error("Feature buffer overflow during preparation."); // Should not happen if sized correctly
                }
            }
        }

        // --- Create Input Tensor ---
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            inputTensorValues.data(),
            inputTensorValues.size(),
            input_node_dims.data(),
            input_node_dims.size()
        );

        // --- Run Inference ---
        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr},
            input_node_names.data(), // Array of C strings
            &input_tensor,           // Pointer to input tensor
            1,                       // Number of input tensors
            output_node_names.data(),// Array of C strings
            output_node_names.size() // Number of output tensors expected
        );

        // --- Post-process Output ---
        if (!output_tensors.empty() && output_tensors[0].IsTensor()) {
            // Assuming the first output tensor contains the prediction
            // And assuming it's a single float value (e.g., shape {1,1})
            const float* outputData = output_tensors[0].GetTensorData<float>();
            size_t outputCount = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

            if (outputCount > 0) {
                 signal.predictedValue = static_cast<double>(outputData[0]); // Get the first output value
                 // If output is probability, confidence might be the value itself or derived
                 // signal.confidence = signal.predictedValue;
            } else {
                 Utils::logMessage("OnnxSignalGenerator Warning: Output tensor is empty.");
            }
        } else {
             Utils::logMessage("OnnxSignalGenerator Warning: Model did not return a valid tensor output.");
        }

    } catch (const Ort::Exception& e) {
        Utils::logMessage("OnnxSignalGenerator Ort::Exception during signal generation: " + std::string(e.what()));
        // Return default HOLD signal
    } catch (const std::exception& e) {
        Utils::logMessage("OnnxSignalGenerator std::exception during signal generation: " + std::string(e.what()));
        // Return default HOLD signal
    }

    return signal; // Return signal (action is still HOLD, value is populated)
}