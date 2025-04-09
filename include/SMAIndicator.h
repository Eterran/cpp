// SMAIndicator.h
#ifndef SMAINDICATOR_H
#define SMAINDICATOR_H

#include "Indicator.h"
#include <vector>
#include <deque>    // Use deque for efficient add/remove at both ends
#include <numeric>  // For std::accumulate (though rolling sum is better)

class SMAIndicator : public Indicator {
private:
    int period;
    std::deque<double> priceBuffer; // Holds last 'period' prices
    std::vector<double> values;     // Stores calculated SMA values
    double currentSum;              // Rolling sum for efficiency

public:
    explicit SMAIndicator(int p);

    void update(const Bar& newBar) override;
    double getValue(int index = 0) const override;
    size_t getCount() const override;
    size_t getMinPeriod() const override;
    bool hasValue(int index = 0) const override; // Override for specific SMA logic
};

#endif // SMAINDICATOR_H