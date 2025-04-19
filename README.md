# CppBacktester

[Docs][reference]

[reference]: https://eterran.github.io/cpp/

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
- ML Model Loading             ✅
- API data fetching (not tested)


## Building the Project

### Prerequisites

- CMake 3.12 or higher
- Visual Studio 2022 (on Windows)
- Python with development headers (if building Python module)
- pybind11 (included in third_party github submodule)
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

## Architecture

This section provides an overview of the main classes in the CppBacktester framework and how they interact in a typical backtest.

### Main Classes

- **DataSource (abstract)**: Defines a common interface for data providers. Implemented by:
  - **CSVDataSource**: Loads market data from CSV files using `CSVParserStep`.
  - **APIDataSource**: Fetches data from external APIs.

- **ParserStep (abstract)**: Base parser step. Specialized by:
  - **CSVParserStep**: Parses CSV input into `Bar` objects.
  - **JSONParserStep**: Parses JSON-formatted data or config.

- **DataLoader**: Orchestrates reading raw data via a `DataSource` and returns a sequence of `Bar` objects.

- **BacktestEngine**: Drives the backtest loop. Innitiate `DataLoader`, `Broker`, and `Strategy`.

- **Broker**: Manages, process order lifecycle, positions, and TP/SL logic.

- **Strategy (abstract)**: Defines decision logic on incoming `Bar` data. Current implementations:
  - **RandomStrategy**: Generates random buy/sell orders with fixed TP/SL for testing.
  - **MLStrategy**: Loads a model via `ModelInterface` and issues orders based on model outputs.

- **ModelInterface (abstract)**: Defines model loading and produce model input/output.
  - **OnnxModelInterface**: Implements `ModelInterface` using ONNX Runtime to load `.onnx` files.

- **TradingMetrics**: Calculates and stores aggregated metrics (sharpe ratio, P&L, drawdown).

Other classes:
- **Bar**: Represents a single market data bar containing open, high, low, close, volume, and any extra cols.
- **Config**: Reads and stores configuration settings from `config.json`.
- **ColumnSpec**: Defines the specification for CSV columns for parsing.
- **Indicator**: Base class for technical indicators computed on bar data.
- **Order**: Represents an order created by the broker during a backtest.
- **Position**: Tracks the state and lifecycle of an open position.
- **Utils**: Collection of utility functions used throughout the framework.

### Workflow

1. **Configuration**: Read settings from `config.json` into `Config`.
2. **Data Loading**: `DataLoader` instantiates a `DataSource` (e.g., `CSVDataSource`).
3. **Engine Setup**: Create `BacktestEngine`, attach a `Broker` and chosen `Strategy`.
4. **Backtest Loop**: For each `Bar`:
   - Strategy issues `TradingSignal`
   - Broker executes orders
   - Metrics updated
5. **Results**: Export performance via `TradingMetrics` or Python bindings.

```

