#include "FeatureMatrix.h"
#include <variant>

FeatureMatrix::FeatureMatrix(const std::vector<Bar>& bars) {
    rows_ = bars.size();
    if (rows_ == 0) return;

    // assume uniform extraColumns size
    size_t extraCols = bars.front().columns.size();
    cols_ = /*7 fixed fields*/ 7 + extraCols;

    matrix_.reserve(rows_);
    for (auto const& bar : bars) {
        std::vector<float> row;
        row.reserve(cols_);

        // extras (double/int/long→float; strings→0)
        for (auto const& ex : bar.columns) {
            row.push_back(static_cast<float>(ex));
        }
        matrix_.push_back(std::move(row));
    }
}

std::vector<float> FeatureMatrix::flat() const {
    std::vector<float> out;
    out.reserve(rows_ * cols_);
    for (auto const& r : matrix_) out.insert(out.end(), r.begin(), r.end());
    return out;
}

std::vector<int64_t> FeatureMatrix::shape() const {
    return { static_cast<int64_t>(rows_), static_cast<int64_t>(cols_) };
}