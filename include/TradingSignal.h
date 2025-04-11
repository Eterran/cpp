// include/TradingSignal.h
#ifndef TRADINGSIGNAL_H
#define TRADINGSIGNAL_H

struct TradingSignal {
    enum Action {
        HOLD,       // Do nothing
        GO_LONG,    // Signal to enter long
        GO_SHORT,   // Signal to enter short
        CLOSE_POSITION // Generic close signal (strategy decides which type based on current pos)
        // CLOSE_LONG, // Could be more specific if needed
        // CLOSE_SHORT
    } action = Action::HOLD;

    // Model Output Interpretation
    double predictedValue = 0.0; // Raw model output (e.g., predicted change, probability, direction score)
    double confidence = 1.0;     // Default confidence

    // Add other potential outputs if models provide them later
};

#endif // TRADINGSIGNAL_H