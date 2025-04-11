// include/ISignalGenerator.h
#ifndef ISIGNALGENERATOR_H
#define ISIGNALGENERATOR_H

#include "TradingSignal.h"
#include "Bar.h"
#include "Config.h" // Pass config pointer
#include <vector>
#include <map>
#include <memory> // For potential shared state

class ISignalGenerator {
public:
    virtual ~ISignalGenerator() = default;

    // Initialize with config and potentially other shared resources
    virtual bool init(const Config* config, const std::vector<Bar>* historyData) = 0;

    // Generate signal based on the latest bar and potentially internal state/history
    // Needs access to historical data for windowing features
    virtual TradingSignal generateSignal(size_t currentBarIndex) = 0;

    // Optional update for internal state if needed before generateSignal
    // virtual void update(const Bar& currentBar) {}
};

#endif // ISIGNALGENERATOR_H