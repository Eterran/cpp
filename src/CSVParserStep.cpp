#include "CSVParserStep.h"
#include "Utils.h"
#include "Bar.h"
#include <sstream>
#include <charconv>
#include <algorithm>
#include "Config.h"

CSVParserStep::CSVParserStep(const Config& cfg, std::vector<ColumnSpec> specs_, const std::string& tsFormat_, char delimiter_)
    : specs(std::move(specs_)), tsFormat(tsFormat_), delimiter(delimiter_), cfg(cfg) {}

bool CSVParserStep::parse(const std::string& record, Bar& bar) const {
    if (record.empty() || record.front() == '{') return false;
    // Split fields
    std::vector<std::string> fields;
    std::string field;
    std::stringstream ss(record);
    while (std::getline(ss, field, delimiter)) {
        // trim whitespace
        field.erase(0, field.find_first_not_of(" \t\r\n"));
        field.erase(field.find_last_not_of(" \t\r\n") + 1);
        fields.push_back(field);
    }

    bar.columns.clear();
    
    // First, find and set the timestamp using specs
    for (auto& spec : specs) {
        if (spec.type == ColumnType::Timestamp && 
            spec.index >= 0 && spec.index < (int)fields.size()) {
            const auto& val = fields[spec.index];
            bar.timestamp = Utils::parseTimestamp(val, tsFormat);
            break;
        }
    }
    
    // Now parse all fields as potential numeric columns, except the timestamp column
    int timestampIndex = -1;
    for (auto& spec : specs) {
        if (spec.type == ColumnType::Timestamp) {
            timestampIndex = spec.index;
            break;
        }
    }
    
    // Process all fields except the timestamp
    for (int i = 0; i < (int)fields.size(); i++) {
        // Skip the timestamp column
        if (i == timestampIndex) continue;
        
        const auto& val = fields[i];
        try {
            double x = std::stod(val);
            bar.columns.push_back(x);
        } catch (...) {
            // Skip non-numeric values
            Utils::logMessage("CSVParserStep: Skipping non-numeric value at column " + std::to_string(i) + ": " + val);
        }
    }
    
    // Only accept a bar if at least one data column was parsed
    if (bar.columns.empty()) {
        return false;
    }
    return true;
}