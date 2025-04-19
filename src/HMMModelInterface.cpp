#include "HMMModelInterface.h"
#include "Utils.h"
#include <iostream>
#include <locale>
#include <codecvt>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <filesystem>
#include "FeatureMatrix.h"

using pybind11::ssize_t;

// Initialize Python interpreter once
static pybind11::scoped_interpreter guard{};

HMMModelInterface::HMMModelInterface() {}
HMMModelInterface::~HMMModelInterface() {}

bool HMMModelInterface::LoadModel(const char_T* modelPath) {

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
    // Acquire GIL for Python interaction
    pybind11::gil_scoped_acquire acquire;
    // Prepare numpy array: shape and strides
    std::vector<pybind11::ssize_t> shape_py;
    for (auto dim : inputShape) shape_py.push_back(static_cast<pybind11::ssize_t>(dim));
    std::vector<pybind11::ssize_t> strides_py(shape_py.size());
    if (!shape_py.empty()) {
        strides_py.back() = sizeof(float);
        for (int i = (int)shape_py.size() - 2; i >= 0; --i) {
            strides_py[i] = strides_py[i + 1] * shape_py[i + 1];
        }
    }
    // Create numpy array from inputData
    pybind11::array arr_in(
        pybind11::buffer_info(
            const_cast<float*>(inputData.data()), 
            sizeof(float), 
            pybind11::format_descriptor<float>::format(),
            static_cast<ssize_t>(shape_py.size()), 
            shape_py, 
            strides_py
    ));
    // Convert to float64 numpy array for HMM predict
    auto np = pybind11::module::import("numpy");
    auto arr_in64 = arr_in.attr("astype")(np.attr("float64"));
    // Call Python model.predict
    pybind11::object result_obj = model_.attr("predict")(arr_in64);

    // Cast result to numpy array of integers (assuming hmmlearn returns int states)
    pybind11::array_t<int64_t> result_arr = pybind11::cast<pybind11::array_t<int64_t>>(result_obj);

    // Convert to vector<float>
    std::vector<float> output;
    output.reserve(result_arr.size());
    for (pybind11::ssize_t i = 0; i < result_arr.size(); ++i) {
        output.push_back(static_cast<float>(result_arr.at(i)));
    }
    return output;
}

// New overload: build flat + shape via FeatureMatrix
std::vector<float> HMMModelInterface::Predict(const std::vector<Bar>& bars) {
    FeatureMatrix fm(bars);
    auto flat = fm.flat();
    auto shape = fm.shape();
    return Predict(flat, shape);
}

void HMMModelInterface::PrintModelInfo() {
    try {
        auto repr = model_.attr("__repr__")().cast<std::string>();
        std::cout << "Model info: " << repr << std::endl;
    } catch (...) {
        std::cout << "Model info unavailable" << std::endl;
    }
}