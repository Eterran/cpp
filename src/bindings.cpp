#include <pybind11/pybind11.h>
#include "main.cpp"
namespace py = pybind11;

PYBIND11_MODULE(cppbacktester_py, m) {
    m.doc() = "CPP backtester Python bindings";

    m.def("main", &main, "Run the main function");
}
