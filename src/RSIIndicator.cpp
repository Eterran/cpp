#include "RSIIndicator.h"

RSIIndicator::RSIIndicator(int p) : period(p), currentGain(0.0), currentLoss(0.0) {
    if (p <= 0) {
        throw std::invalid_argument("RSIIndicator: Period must be positive.");
    }
    // Pre-allocate vector space? Optional optimization.
    // values.reserve(estimated_data_size);
}

void RSIIndicator::update(const Bar& newBar){
    double price = newBar.close; // Use close price for RSI calculation

    priceBuffer.push_back(price);
    if (priceBuffer.size() < 2) {
        // Not enough data to calculate gain/loss yet
        return;
    }
    double change = price - priceBuffer[priceBuffer.size() - 2]; // Current - Previous
    if (change > 0) {
        currentGain += change;
        currentLoss = 0.0; // Reset loss
    } else if (change < 0) {
        currentLoss -= change; // Change is negative, so subtract to get positive loss
        currentGain = 0.0; // Reset gain
    }

    // If buffer exceeds period, remove the oldest element and adjust gain/loss
    if (priceBuffer.size() > period) {
        double oldestPrice = priceBuffer.front();
        priceBuffer.pop_front();

        // Calculate change for the oldest price
        double oldestChange = oldestPrice - priceBuffer.back(); // Newest - Oldest
        if (oldestChange > 0) {
            currentGain -= oldestChange;
            currentLoss = 0.0; // Reset loss
        } else if (oldestChange < 0) {
            currentLoss += -oldestChange; // Change is negative, so subtract to get positive loss
            currentGain = 0.0; // Reset gain
        }
    }


    // Calculate and store RSI value only when buffer is full
    if (priceBuffer.size() == period) {
        double avgGain = currentGain / period;
        double avgLoss = currentLoss / period;

        if (avgLoss == 0) {
            values.push_back(100); // Avoid division by zero, RSI is 100 if no loss
        } else {
            double rs = avgGain / avgLoss;
            double rsi = 100 - (100 / (1 + rs));
            values.push_back(rsi);
        }
    } else {
        // Not enough data yet, push NaN or handle differently?
        // Pushing NaN ensures values vector size matches bar count after min period
        // values.push_back(NaN); // Or don't push anything until valid
        // Let's NOT push until valid, makes getCount() reflect valid count
    }
};

double RSIIndicator::getValue(int index) const{
    if(!hasValue(index)) {
        return NaN; // Return NaN if value doesn't exist or isn't calculated yet
    }
    // index 0 is the most recent value, which is at the back of the vector
    return values[values.size() - 1 - index];
};

size_t RSIIndicator::getCount() const{
    // Returns the number of *valid* RSI values calculated
    return values.size();
};

size_t RSIIndicator::getMinPeriod() const{
    return period;
};

bool RSIIndicator::hasValue(int index) const{
    // Check if enough values calculated overall AND if requested index is valid
    size_t numValidValues = values.size();
    return (numValidValues > 0) && (index >= 0) && (static_cast<size_t>(index) < numValidValues);
};
