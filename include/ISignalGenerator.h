// include/ISignalGenerator.h
#ifndef ISIGNALGENERATOR_H
#define ISIGNALGENERATOR_H

#include "TradingSignal.h"
#include "Bar.h"
#include "Config.h"
#include <vector>
#include <map>
#include <memory>

class ISignalGenerator {
public:
    virtual ~ISignalGenerator() = default;

    // Initialize with config and historical data pointer. Return true on success.
    virtual bool init(const Config* config, const std::vector<Bar>* historyData) = 0;

    // Generate signal based on the bar at the given index in historyData.
    virtual TradingSignal generateSignal(size_t currentBarIndex) = 0;

    // Optional: Update internal state based on the *latest* bar (called before generateSignal)
    // virtual void update(const Bar& currentBar) {}
};

#endif // ISIGNALGENERATOR_H