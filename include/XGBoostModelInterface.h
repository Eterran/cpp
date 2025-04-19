#pragma once

#include "ModelInterface.h"
#include <vector>
#include <xgboost/c_api.h>

class XGBoostModelInterface : public ModelInterface {
private:
    std::vector<BoosterHandle> boosters_;

public:
    XGBoostModelInterface();
    ~XGBoostModelInterface() override;

    bool LoadModel(const char_T* modelPath) override;
    std::vector<float> Predict(const std::vector<float>& inputData,
                               const std::vector<int64_t>& inputShape) override;
    void PrintModelInfo() override;
};