#pragma once
#include <vector>
#include "Bar.h"

class FeatureMatrix {
public:
    // Build feature rows from bars: [open, high, low, close, bid, ask, volume, extra...]
    explicit FeatureMatrix(const std::vector<Bar>& bars);

    // 2D access: rows×cols
    const std::vector<std::vector<float>>& matrix() const { return matrix_; }

    // Flatten into one big row-major vector
    std::vector<float> flat() const;

    // { rows, cols } for Python/NumPy shape
    std::vector<int64_t> shape() const;

private:
    std::vector<std::vector<float>> matrix_;
    size_t rows_{0}, cols_{0};
};