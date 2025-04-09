// Indicator.h
#ifndef INDICATOR_H
#define INDICATOR_H

#include "Bar.h" // Needs Bar definition for update method
#include <vector>
#include <string>
#include <limits> // For quiet_NaN

// Abstract base class for all indicators
class Indicator
{
public:
    virtual ~Indicator() = default; // Virtual destructor is essential for base classes

    // Update the indicator with the latest bar data
    virtual void update(const Bar &newBar) = 0;

    // Get the calculated indicator value.
    // index = 0: current value
    // index = 1: previous value, etc.
    virtual double getValue(int index = 0) const = 0;

    // Get the number of calculated values available
    virtual size_t getCount() const = 0;

    // Get the minimum number of bars required before the indicator produces valid output
    virtual size_t getMinPeriod() const = 0;

    // Check if the indicator has enough data to produce a valid value at the specified index
    virtual bool hasValue(int index = 0) const
    {
        // Default implementation: checks if enough values have been calculated overall
        // And if the requested index is within the bounds of calculated values.
        return index >= 0 && static_cast<size_t>(index) < getCount();
    }

protected:
    // Helper for returning NaN when value is not available
    static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
};

#endif // INDICATOR_H