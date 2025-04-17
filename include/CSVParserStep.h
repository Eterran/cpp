#ifndef CSVPARSERSTEP_H
#define CSVPARSERSTEP_H

#include "ParserStep.h"
#include "Config.h"
#include <string>

// Parser step for CSV-formatted records
class CSVParserStep : public ParserStep {
public:
    explicit CSVParserStep(const Config& config);
    bool parse(const std::string& record, Bar& bar) const override;

private:
    const Config& config;
};

#endif // CSVPARSERSTEP_H