#ifndef RSIINDICATOR_H
#define RSIINDICATOR_H

#include "Indicator.h"
#include <vector>
#include <deque>    // Use deque for efficient add/remove at both ends
#include <numeric>  // For std::accumulate (though rolling sum is better)
#include <stdexcept> // For invalid_argument

class RSIIndicator : public Indicator {
private:
    int period;
    std::deque<double> priceBuffer; // Holds last 'period' prices
    std::vector<double> values;     // Stores calculated RSI values
    double currentGain;             // Rolling gain for efficiency
    double currentLoss;             // Rolling loss for efficiency

public:
    explicit RSIIndicator(int p);

    void update(const Bar& newBar) override;
    double getValue(int index = 0) const override;
    size_t getCount() const override;
    size_t getMinPeriod() const override;
    bool hasValue(int index = 0) const override; // Override for specific RSI logic
};

#endif