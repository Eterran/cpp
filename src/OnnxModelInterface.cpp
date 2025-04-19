// #include <iostream>
// #include <vector>
// #include <string>
// #include <memory>
// #include <onnxruntime_cxx_api.h>
// #include "OnnxModelInterface.h"

// // Define char_T if not defined elsewhere
// #ifdef _WIN32
// using char_T = wchar_t;
// #else
// using char_T = char;
// #endif

// // ONNX implementation of the ModelInterface
// OnnxModelInterface::OnnxModelInterface() {
// #ifdef _WIN32
//     env_ = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "OnnxModel".);
// #else
//     env_ = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "OnnxModel");
// #endif
//     session_ = nullptr;
//     allocator_ = new Ort::AllocatorWithDefaultOptions();
// }

// OnnxModelInterface::~OnnxModelInterface() {
//     delete env_;
//     delete session_;
//     delete allocator_;
// }

// bool OnnxModelInterface::LoadModel(const char_T* modelPath) {
//     // Validate model extension
//     std::string ext;
// #ifdef _WIN32
//     size_t len = wcslen(modelPath);
//     if (len < 5) { std::cerr << "Invalid model path length" << std::endl; return false; }
//     std::wstring wext(modelPath + len - 5, modelPath + len);
//     // Convert wide extension to narrow string safely
//     ext.clear();
//     ext.reserve(wext.size());
//     for (wchar_t wc : wext) ext.push_back(static_cast<char>(wc));
// #else
//     std::string pathStr(modelPath);
//     if (pathStr.size() < 5) { std::cerr << "Invalid model path length" << std::endl; return false; }
//     ext = pathStr.substr(pathStr.size() - 5);
// #endif
//     for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
//     if (ext != ".onnx" && ext != ".ort") {
//         std::cerr << "Unsupported model type: " << ext << std::endl;
//         return false;
//     }
//     try {
//         Ort::SessionOptions sessionOptions;
//         sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
//         delete session_; // Delete old session if exists
//         session_ = new Ort::Session(*env_, modelPath, sessionOptions);
//         // Prime model input and output names for faster inference
//         {
//             auto inp = session_->GetInputNameAllocated(0, *allocator_);
//             inputName_ = inp.get();
//         }
//         {
//             auto out = session_->GetOutputNameAllocated(0, *allocator_);
//             outputName_ = out.get();
//         }
//         std::cout << "Model loaded successfully: " << modelPath << std::endl;
//         return true;
//     } catch (const Ort::Exception& e) {
//         std::cerr << "Error loading model: " << e.what() << std::endl;
//         return false;
//     }
// }

// std::vector<float> OnnxModelInterface::Predict(const std::vector<float>& inputData, const std::vector<int64_t>& inputShape) {
//     try {
//         // Create input tensor
//         Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
//         Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
//             memoryInfo,
//             const_cast<float*>(inputData.data()), 
//             inputData.size(), 
//             inputShape.data(), 
//             inputShape.size()
//         );
        
//         // Run inference using primed names
//         const char* inputNames[] = {inputName_.c_str()};
//         const char* outputNames[] = {outputName_.c_str()};
        
//         auto outputTensors = session_->Run(
//             Ort::RunOptions{nullptr}, 
//             inputNames, 
//             &inputTensor, 
//             1, 
//             outputNames, 
//             1
//         );
        
//         // Get output data
//         float* outputData = outputTensors.front().GetTensorMutableData<float>();
//         size_t outputSize = outputTensors.front().GetTensorTypeAndShapeInfo().GetElementCount();
        
//         return std::vector<float>(outputData, outputData + outputSize);
//     } catch (const Ort::Exception& e) {
//         std::cerr << "Error during prediction: " << e.what() << std::endl;
//         return {};
//     }
// }

// void OnnxModelInterface::PrintModelInfo() {
//     if (!session_) {
//         std::cout << "No model loaded." << std::endl;
//         return;
//     }
    
//     size_t numInputs = session_->GetInputCount();
//     size_t numOutputs = session_->GetOutputCount();
    
//     std::cout << "Number of inputs: " << numInputs << std::endl;
//     std::cout << "Number of outputs: " << numOutputs << std::endl;
// }
