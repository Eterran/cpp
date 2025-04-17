#ifndef JSONPARSERSTEP_H
#define JSONPARSERSTEP_H

#include "ParserStep.h"
#include "ColumnSpec.h"
#include <vector>
#include <string>

// Parser step for JSON-formatted records driven by ColumnSpec
class JSONParserStep : public ParserStep {
public:
    JSONParserStep(std::vector<ColumnSpec> specs, const std::string& tsFormat);
    bool parse(const std::string& record, Bar& bar) const override;

private:
    std::vector<ColumnSpec> specs;
    std::string tsFormat;
};

#endif // JSONPARSERSTEP_H