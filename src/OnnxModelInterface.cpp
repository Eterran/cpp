#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include "../include/OnnxModelInterface.h"

// Define char_T if not defined elsewhere
#ifdef _WIN32
using char_T = wchar_t;
#else
using char_T = char;
#endif

// ONNX implementation of the ModelInterface
OnnxModelInterface::OnnxModelInterface() : 
    env_(new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "OnnxModel")),
    session_(nullptr),
    allocator_(new Ort::AllocatorWithDefaultOptions()) {}

OnnxModelInterface::~OnnxModelInterface() {
    delete env_;
    delete session_;
    delete allocator_;
}

bool OnnxModelInterface::LoadModel(const char_T* modelPath) {
    try {
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
        
        // Fix: Create a new session instead of dereferencing nullptr
        delete session_; // Delete old session if exists
        session_ = new Ort::Session(*env_, modelPath, sessionOptions);
        
        std::cout << "Model loaded successfully: " << modelPath << std::endl;
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        return false;
    }
}

std::vector<float> OnnxModelInterface::Predict(const std::vector<float>& inputData, const std::vector<int64_t>& inputShape) {
    try {
        // Get input and output names - use arrow notation for pointers
        Ort::AllocatedStringPtr inputName = session_->GetInputNameAllocated(0, *allocator_);
        Ort::AllocatedStringPtr outputName = session_->GetOutputNameAllocated(0, *allocator_);
        
        // Create input tensor
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, 
            const_cast<float*>(inputData.data()), 
            inputData.size(), 
            inputShape.data(), 
            inputShape.size()
        );
        
        // Run inference
        const char* inputNames[] = {inputName.get()};
        const char* outputNames[] = {outputName.get()};
        
        auto outputTensors = session_->Run(
            Ort::RunOptions{nullptr}, 
            inputNames, 
            &inputTensor, 
            1, 
            outputNames, 
            1
        );
        
        // Get output data
        float* outputData = outputTensors.front().GetTensorMutableData<float>();
        size_t outputSize = outputTensors.front().GetTensorTypeAndShapeInfo().GetElementCount();
        
        return std::vector<float>(outputData, outputData + outputSize);
    } catch (const Ort::Exception& e) {
        std::cerr << "Error during prediction: " << e.what() << std::endl;
        return {};
    }
}

void OnnxModelInterface::PrintModelInfo() {
    if (!session_) {
        std::cout << "No model loaded." << std::endl;
        return;
    }
    
    size_t numInputs = session_->GetInputCount();
    size_t numOutputs = session_->GetOutputCount();
    
    std::cout << "Number of inputs: " << numInputs << std::endl;
    std::cout << "Number of outputs: " << numOutputs << std::endl;
}

// Implementation of the factory method to create appropriate model interfaces
std::unique_ptr<ModelInterface> ModelInterface::CreateModel(const char_T* modelPath) {
    std::string ext;
    
    #ifdef _WIN32
    // Convert wstring to string safely for extension extraction
    size_t pathLen = wcslen(modelPath);
    if (pathLen < 5) { // Need at least 5 chars for ".onnx"
        std::cerr << "Invalid model path length" << std::endl;
        return nullptr;
    }
    // Extract extension directly from wide string
    std::wstring wext(modelPath + pathLen - 4, modelPath + pathLen);
    // Convert wide extension to narrow safely
    ext.resize(wext.length());
    for (size_t i = 0; i < wext.length(); ++i) {
        ext[i] = static_cast<char>(wext[i] & 0xFF);
    }
    #else
    std::string pathStr(modelPath);
    ext = pathStr.substr(pathStr.length() - 4);
    #endif
    
    // Convert to lowercase for case-insensitive comparison
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    
    if (ext == ".ort" || ext == "onnx") {
        return std::make_unique<OnnxModelInterface>();
    }
    
    // Add more model types here as they're implemented
    // else if (ext == ".lstm") { return std::make_unique<LstmModelInterface>(); }
    // else if (ext == ".xgb") { return std::make_unique<XgboostModelInterface>(); }
    
    std::cerr << "Unsupported model type: " << ext << std::endl;
    return nullptr;
}
