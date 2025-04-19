#include "XGBoostModelInterface.h"
#include <iostream>
#include <limits>
#include "Utils.h"

XGBoostModelInterface::XGBoostModelInterface() {
}

XGBoostModelInterface::~XGBoostModelInterface() {
    // Free all loaded boosters
    for (auto &booster : boosters_) {
        XGBoosterFree(booster);
    }
}

// Load XGBoost model and store handle
bool XGBoostModelInterface::LoadModel(const char_T* modelPath) {
    BoosterHandle handle;
    if (XGBoosterCreate(nullptr, 0, &handle) != 0) {
        std::cerr << "Failed to create XGBoost booster" << std::endl;
        return false;
    }

    #ifdef _WIN32
    std::wstring wpath(modelPath);
    std::string path = Utils::WideToUTF8(wpath);
    const char* cpath = path.c_str();
    #else
        const char* cpath = modelPath;
    #endif
    if (XGBoosterLoadModel(handle, cpath) != 0) {
        std::cerr << "Failed to load XGBoost model from " << cpath << std::endl;
        XGBoosterFree(handle);
        return false;
    }
    boosters_.push_back(handle);
    return true;
}

// Run prediction on the last loaded model
std::vector<float> XGBoostModelInterface::Predict(const std::vector<float>& inputData,
                                                  const std::vector<int64_t>& inputShape) {
    if (boosters_.empty()) {
        std::cerr << "No XGBoost model loaded" << std::endl;
        return {};
    }
    // Determine rows and columns
    bst_ulong nRows = inputShape.empty() ? 1 : static_cast<bst_ulong>(inputShape[0]);
    bst_ulong nCols = static_cast<bst_ulong>(inputData.size()) / (nRows == 0 ? 1 : nRows);
    DMatrixHandle dmat;
    float missing = std::numeric_limits<float>::quiet_NaN();
    if (XGDMatrixCreateFromMat(inputData.data(), nRows, nCols, missing, &dmat) != 0) {
        std::cerr << "Failed to create DMatrix for prediction" << std::endl;
        return {};
    }
    bst_ulong out_len = 0;
    const float* out_result = nullptr;
    // option_mask=0, ntree_limit=0 (all trees), training=0 (inference)
    if (XGBoosterPredict(boosters_.back(), dmat, 0, 0u, 0, &out_len, &out_result) != 0) {
        std::cerr << "XGBoost prediction failed" << std::endl;
        XGDMatrixFree(dmat);
        return {};
    }
    std::vector<float> predictions(out_result, out_result + out_len);
    XGDMatrixFree(dmat);
    return predictions;
}

std::vector<float> XGBoostModelInterface::predict2D(const std::vector<std::vector<float>> inputData){
    if (inputData.empty()) {
        return {};
    }
    
    // Flatten the 2D input data into a 1D vector
    size_t rows = inputData.size();
    size_t cols = inputData[0].size();
    
    std::vector<float> flatData;
    flatData.reserve(rows * cols);
    
    for (const auto& row : inputData) {
        if (row.size() != cols) {
            std::cerr << "Error: Inconsistent row sizes in 2D input data" << std::endl;
            return {};
        }
        flatData.insert(flatData.end(), row.begin(), row.end());
    }
    
    // Define the shape for the flattened data
    std::vector<int64_t> shape = {static_cast<int64_t>(rows), static_cast<int64_t>(cols)};
    
    // Use the standard Predict method with the flattened data
    return Predict(flatData, shape);
}

void XGBoostModelInterface::PrintModelInfo() {
    std::cout << "Loaded " << boosters_.size() << " XGBoost models" << std::endl;
}