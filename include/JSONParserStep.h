#ifndef JSONPARSERSTEP_H
#define JSONPARSERSTEP_H

#include "ParserStep.h"
#include "Config.h"
#include <string>

// Parser step for JSON-formatted records
class JSONParserStep : public ParserStep {
public:
    explicit JSONParserStep(const Config& config);
    bool parse(const std::string& record, Bar& bar) const override;

private:
    const Config& config;
};

#endif // JSONPARSERSTEP_H