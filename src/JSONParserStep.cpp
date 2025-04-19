#include "JSONParserStep.h"
#include "Utils.h"
#include "Bar.h"
#include "json.hpp"

JSONParserStep::JSONParserStep(std::vector<ColumnSpec> specs_, const std::string& tsFormat_)
    : specs(std::move(specs_)), tsFormat(tsFormat_) {}

bool JSONParserStep::parse(const std::string& record, Bar& bar) const {
    if (record.empty() || record.front() != '{') return false;
    try {
        auto obj = nlohmann::json::parse(record);

        bar.columns.clear();
        // Parse each spec
        for (auto& spec : specs) {
            const auto& key = spec.name;
            if (!obj.contains(key)) continue;
            const auto& v = obj.at(key);
            switch (spec.type) {
            case ColumnType::Timestamp:
                bar.timestamp = Utils::parseTimestamp(v.get<std::string>(), tsFormat);
                break;
            case ColumnType::Extra:
                bar.columns.push_back(v.get<double>());
            }
        }
        // Only accept if at least one data column parsed
        if (bar.columns.empty()) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}