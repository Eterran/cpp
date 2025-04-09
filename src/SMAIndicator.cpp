// SMAIndicator.cpp
#include "SMAIndicator.h"
#include <stdexcept> // For invalid_argument

SMAIndicator::SMAIndicator(int p) : period(p), currentSum(0.0) {
    if (p <= 0) {
        throw std::invalid_argument("SMAIndicator: Period must be positive.");
    }
    // Pre-allocate vector space? Optional optimization.
    // values.reserve(estimated_data_size);
}

void SMAIndicator::update(const Bar& newBar) {
    double price = newBar.close; // Use close price for SMA calculation

    priceBuffer.push_back(price);
    currentSum += price;

    // If buffer exceeds period, remove the oldest element and subtract it from sum
    if (priceBuffer.size() > period) {
        currentSum -= priceBuffer.front();
        priceBuffer.pop_front();
    }

    // Calculate and store SMA value only when buffer is full
    if (priceBuffer.size() == period) {
        values.push_back(currentSum / period);
    } else {
        // Not enough data yet, push NaN or handle differently?
        // Pushing NaN ensures values vector size matches bar count after min period
        // values.push_back(NaN); // Or don't push anything until valid
        // Let's NOT push until valid, makes getCount() reflect valid count
    }
}

double SMAIndicator::getValue(int index) const {
    if (!hasValue(index)) {
        return NaN; // Return NaN if value doesn't exist or isn't calculated yet
    }
    // index 0 is the most recent value, which is at the back of the vector
    return values[values.size() - 1 - index];
}

size_t SMAIndicator::getCount() const {
    // Returns the number of *valid* SMA values calculated
    return values.size();
}

size_t SMAIndicator::getMinPeriod() const {
    // Need 'period' bars to calculate the first SMA value
    return period;
}

bool SMAIndicator::hasValue(int index) const {
    // Check if enough values calculated overall AND if requested index is valid
    size_t numValidValues = values.size();
    return (numValidValues > 0) && (index >= 0) && (static_cast<size_t>(index) < numValidValues);
}