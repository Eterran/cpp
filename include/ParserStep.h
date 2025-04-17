#ifndef PARSERSTEP_H
#define PARSERSTEP_H

#include <string>
#include "Bar.h"

// Interface for a parsing step that tries to parse a record into a Bar
class ParserStep {
public:
    virtual ~ParserStep() = default;
    // Returns true if parsing succeeded, false otherwise
    virtual bool parse(const std::string& record, Bar& bar) const = 0;
};

#endif // PARSERSTEP_H