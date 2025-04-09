#include "Strategy.h"

// Implement the pure virtual method to avoid linker errors
// If this is already implemented elsewhere, make sure this file is being compiled

std::string Strategy::getName() const {
    // This should be overridden by derived classes, 
    // but we need to provide a base implementation
    return "Base Strategy";
}

// Any other missing method implementations from Strategy class should go here
