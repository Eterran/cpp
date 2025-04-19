#ifndef CSVPARSERSTEP_H
#define CSVPARSERSTEP_H

#include "ParserStep.h"
#include "ColumnSpec.h"
#include "Config.h"
#include "Bar.h"
#include <string>
#include <vector>

// Parser step for CSV-formatted records
class CSVParserStep : public ParserStep {
public:
    CSVParserStep(const Config& cfg, std::vector<ColumnSpec> specs, const std::string& tsFormat, char delimiter);
    bool parse(const std::string& record, Bar& bar) const override;

private:
    std::vector<ColumnSpec> specs;
    std::string tsFormat;
    char delimiter;
    const Config& cfg;
};

#endif // CSVPARSERSTEP_H