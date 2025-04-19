#include "HMMModelInterface.h"
#include "Utils.h"
#include <iostream>
#include <locale>
#include <codecvt>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/embed.h>
#include <filesystem>

// Initialize Python interpreter once
static pybind11::scoped_interpreter guard{};

HMMModelInterface::HMMModelInterface() {}
HMMModelInterface::~HMMModelInterface() {}

bool HMMModelInterface::LoadModel(const char_T* modelPath) {
    std::cout << std::filesystem::current_path();


    // Diagnose embedded Python
    {
        auto sys = pybind11::module::import("sys");
        auto exe = sys.attr("executable").cast<std::string>();
        auto sp = sys.attr("path");
        auto sp_repr = sp.attr("__repr__")().cast<std::string>();
        std::cout << "[HMMModelInterface] Embedded Python exec: " << exe << std::endl;
        std::cout << "[HMMModelInterface] sys.path: " << sp_repr << std::endl;
    }
    // Ensure hmmlearn  on path
    try {
        auto site = pybind11::module::import("site");
        site.attr("addsitedir")("E:\\Python312\\Lib\\site-packages");
        std::cout << "[HMMModelInterface] Added site-packages to sys.path" << std::endl;
    } catch (...) {
        std::cerr << "[HMMModelInterface] Warning: Could not add site-packages to sys.path" << std::endl;
    }


    try {
        std::string path;
    #ifdef _WIN32
            std::wstring wpath(modelPath);
            path = Utils::WideToUTF8(wpath);
    #else
            path = modelPath;
    #endif
        auto pickle = pybind11::module::import("pickle");
        auto builtins = pybind11::module::import("builtins");
        auto file = builtins.attr("open")(path, "rb");
        model_ = pickle.attr("load")(file);
        return true;
    } catch (const std::exception &e) {
        std::cerr << "HMMModelInterface Error loading model: " << e.what() << std::endl;
        return false;
    }
}

std::vector<float> HMMModelInterface::predict2D(const std::vector<std::vector<float>> inputData){
    size_t rows = inputData.size();
    size_t cols = inputData.empty() ? 0 : inputData[0].size();

    std::vector<float> flat;
    flat.reserve(rows * cols);
    for (auto &row : inputData)
        flat.insert(flat.end(), row.begin(), row.end());

    std::vector<int64_t> shape = { (int64_t)rows, (int64_t)cols };

    auto out = this->Predict(flat, shape);
    return out;
}

// flatten into a vector for input data
std::vector<float> HMMModelInterface::Predict(const std::vector<float>& inputData,
                                                 const std::vector<int64_t>& inputShape) {
    pybind11::array_t<float> arr(inputShape, inputData.data());
    auto result = model_.attr("predict")(arr);
    return result.cast<std::vector<float>>();
}

void HMMModelInterface::PrintModelInfo() {
    try {
        auto repr = model_.attr("__repr__")().cast<std::string>();
        std::cout << "Model info: " << repr << std::endl;
    } catch (...) {
        std::cout << "Model info unavailable" << std::endl;
    }
}