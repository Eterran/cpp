#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <cstring>
#include "Bar.h"
#include "Config.h"
#include "DataLoader.h"
#include "main.cpp"

namespace py = pybind11;

PYBIND11_MODULE(cppbacktester_py, m) {
    m.doc() = "CPP backtester Python bindings";

    // Existing binding
    m.def("main", &main, "Run the main function");

    // Bar struct binding
    py::class_<Bar>(m, "Bar")
        .def(py::init<>())
        .def_readwrite("timestamp", &Bar::timestamp)
        .def_readwrite("columns", &Bar::columns);

    // Config class binding
    py::class_<Config>(m, "Config")
        .def(py::init<>())
        .def("load_from_file", &Config::loadFromFile, "Load configuration from a JSON file");

    // DataLoader class binding
    py::class_<DataLoader>(m, "DataLoader")
        .def(py::init<const Config&>())
        .def("load_data", &DataLoader::loadData,
             py::arg("use_partial") = false,
             py::arg("partial_percent") = 100.0,
             "Load data as a list of Bar objects");

    // Model loading and prediction bindings
    m.def("load_model", [](const std::string &path) {
        py::object pickle = py::module::import("pickle");
        py::object builtins = py::module::import("builtins");
        py::object file = builtins.attr("open")(path, "rb");
        py::object model = pickle.attr("load")(file);
        return model;
    }, py::arg("path"), "Load Python pickle model");

    // // HMMModelInterface wrapper
    // py::class_<HMMModelInterface>(m, "HMMModelInterface")
    //     .def(py::init<>())
    //     .def("load_model", &HMMModelInterface::LoadModel)
    //     .def("predict", [](HMMModelInterface &self, const std::vector<Bar> &bars) {
    //         auto result = self.Predict(bars);
    //         py::array_t<float> arr(result.size());
    //         std::memcpy(arr.mutable_data(), result.data(), result.size()*sizeof(float));
    //         return arr;
    //     })
    //     .def("predict2D", [](HMMModelInterface &self, const std::vector<std::vector<float>> &inputData) {
    //         auto result = self.predict2D(inputData);
    //         py::ssize_t rows = (py::ssize_t)inputData.size();
    //         py::ssize_t cols = (py::ssize_t)(inputData.empty() ? 0 : inputData[0].size());
    //         py::array_t<float> arr({rows, cols});
    //         std::memcpy(arr.mutable_data(), result.data(), rows*cols*sizeof(float));
    //         return arr;
    //     });

    // remove or deprecate old predict_model
    // m.def("predict_model", [](py::object model, const std::vector<Bar> &bars) {
    //     py::list py_bars = py::list::create();
    //     for (const auto &bar : bars) {
    //         py::dict d;
    //         d["timestamp"] = bar.timestamp;
    //         d["open"] = bar.open;
    //         d["high"] = bar.high;
    //         d["low"] = bar.low;
    //         d["close"] = bar.close;
    //         d["bid"] = bar.bid;
    //         d["ask"] = bar.ask;
    //         d["volume"] = bar.volume;
    //         d["extra_columns"] = bar.extraColumns;
    //         py_bars.append(d);
    //     }
    //     py::object result = model.attr("predict")(py_bars);
    //     return result.cast<std::vector<double>>();
    // }, py::arg("model"), py::arg("bars"), "Predict using loaded model");
}
