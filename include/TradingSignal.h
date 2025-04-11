// include/TradingSignal.h
#ifndef TRADINGSIGNAL_H
#define TRADINGSIGNAL_H

struct TradingSignal {
    enum Action {
        HOLD,       // Do nothing
        GO_LONG,    // Signal to enter long
        GO_SHORT,   // Signal to enter short
        CLOSE_LONG, // Signal to exit long (distinct from GO_SHORT)
        CLOSE_SHORT // Signal to exit short (distinct from GO_LONG)
        // Could add: INCREASE_LONG, DECREASE_SHORT etc.
    } action = Action::HOLD;

    // Model Output Interpretation (Examples)
    double predictedValue = 0.0; // Raw model output (e.g., predicted change, probability)
    double confidence = 1.0;     // Confidence score (if model provides it)

    // Optional targets directly from signal (less common for simple change models)
    // double targetPrice = 0.0;
    // double stopLossPrice = 0.0;
};

#endif // TRADINGSIGNAL_H
