# CppBacktester

A C++ backtesting framework with Python bindings using pybind11 and ONNX Runtime.

## Features

- Load and parse CSV           ✅
- Load and save json config    ✅
- Bar/data format              ✅
- Backtesting engine           ✅
- Broker class to manage order ✅
- Strategy class               ✅
- Run a back test              ✅
- Calculate metrics            ✅
- ML Model Loading (in progress)
- API data fetching (planned)

## Building the Project

### Prerequisites

- CMake 3.12 or higher
- Visual Studio 2022 (on Windows)
- Python with development headers (if building Python module)
- pybind11 (included in third_party)
- ONNX Runtime (automatically downloaded if not found)
- vcpkg (to install curl with openssl)
  vcpkg install curl[core,ssl] --triplet x64-windows-static 

### Build Options

The project has two main build targets:
- C++ executable (`BUILD_EXECUTABLE=ON`)
- Python module (`BUILD_PYTHON_MODULE=ON`)

Both are enabled by default.

### Building with CMake

```bash
# Create and enter build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release
```

### Using the Helper Script

For convenience, you can use the provided batch script to clean and rebuild the project:

```bash
# On Windows
build.bat
```

## Directory Structure

After building, the output files will be organized as follows:

```
build/
  output/
    bin/
      backtest_executable.exe  # C++ executable
      *.dll                    # Shared libraries (ONNX Runtime)
    lib/
      cppbacktester_py.pyd     # Python module
```

