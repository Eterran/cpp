#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
// Pybind11 embed and numpy includes moved here
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
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
        .def_readwrite("open", &Bar::open)
        .def_readwrite("high", &Bar::high)
        .def_readwrite("low", &Bar::low)
        .def_readwrite("close", &Bar::close)
        .def_readwrite("bid", &Bar::bid)
        .def_readwrite("ask", &Bar::ask)
        .def_readwrite("volume", &Bar::volume)
        .def_readwrite("extra_columns", &Bar::extraColumns);

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

    m.def("predict_model", [](py::object model, const std::vector<Bar> &bars) {
        py::list py_bars;
        for (const auto &bar : bars) {
            py::dict d;
            d["timestamp"] = bar.timestamp;
            d["open"] = bar.open;
            d["high"] = bar.high;
            d["low"] = bar.low;
            d["close"] = bar.close;
            d["bid"] = bar.bid;
            d["ask"] = bar.ask;
            d["volume"] = bar.volume;
            d["extra_columns"] = bar.extraColumns;
            py_bars.append(d);
        }
        py::object result = model.attr("predict")(py_bars);
        return result.cast<std::vector<double>>();
    }, py::arg("model"), py::arg("bars"), "Predict using loaded model");
}
