#pragma once

#include <vector>
#include <string>
#include <memory>

// Define char_T if not defined elsewhere
#ifdef _WIN32
using char_T = wchar_t;
#else
using char_T = char;
#endif

// Abstract base class for all model interfaces
class ModelInterface {
public:
    ModelInterface() = default;
    virtual ~ModelInterface() = default;

    // Load a model from a file path
    virtual bool LoadModel(const char_T* modelPath) = 0;

    // Run inference on input data and return predictions
    virtual std::vector<float> Predict(const std::vector<float>& inputData, 
                                      const std::vector<int64_t>& inputShape) = 0;
    
    // Print information about the model
    virtual void PrintModelInfo() = 0;

    // Factory method to create model instances based on file extension
    static std::unique_ptr<ModelInterface> CreateModel(const char_T* modelPath);
};
