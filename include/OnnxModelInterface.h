// #pragma once

// #include <vector>
// #include <memory>
// #include <string>
// #include "ModelInterface.h"

// // Forward declarations for ONNX Runtime types
// namespace Ort {
//     struct Env;
//     struct Session;
//     struct AllocatorWithDefaultOptions;
// }

// // Define char_T if not defined elsewhere
// #ifdef _WIN32
// using char_T = wchar_t;
// #else
// using char_T = char;
// #endif

// // ONNX implementation of the ModelInterface
// class OnnxModelInterface : public ModelInterface {
// private:
//     Ort::Env* env_;
//     Ort::Session* session_;
//     Ort::AllocatorWithDefaultOptions* allocator_;
//     // Primed input/output names for inference
//     std::string inputName_;
//     std::string outputName_;

// public:
//     OnnxModelInterface();
//     ~OnnxModelInterface() override;
    
//     bool LoadModel(const char_T* modelPath);
//     std::vector<float> Predict(const std::vector<float>& inputData, const std::vector<int64_t>& inputShape) override;
//     void PrintModelInfo() override;
// };
