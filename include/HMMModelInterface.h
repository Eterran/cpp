#pragma once
#include "ModelInterface.h"
#include <vector>
#include <string>
#include <pybind11/pybind11.h>

class HMMModelInterface : public ModelInterface {
public:
    HMMModelInterface();
    ~HMMModelInterface() override;

    bool LoadModel(const char_T* modelPath) override;
    std::vector<float> Predict(const std::vector<float>& inputData,
                               const std::vector<int64_t>& inputShape) override;
    
    std::vector<float> predict2D(const std::vector<std::vector<float>> inputData);
  
    void PrintModelInfo() override;

private:
    pybind11::object model_;
};