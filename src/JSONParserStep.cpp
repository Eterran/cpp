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
        // Presence flags
        bool hasOpen=false, hasHigh=false, hasLow=false, hasClose=false, hasBid=false, hasAsk=false, hasVol=false;
        bar.extraColumns.clear();
        // Parse each spec
        for (auto& spec : specs) {
            const auto& key = spec.name;
            if (!obj.contains(key)) continue;
            const auto& v = obj.at(key);
            switch (spec.type) {
            case ColumnType::Timestamp:
                bar.timestamp = Utils::parseTimestamp(v.get<std::string>(), tsFormat);
                break;
            case ColumnType::Open:
                if (v.is_number()) { bar.open = v.get<double>(); hasOpen=true; }
                break;
            case ColumnType::High:
                if (v.is_number()) { bar.high = v.get<double>(); hasHigh=true; }
                break;
            case ColumnType::Low:
                if (v.is_number()) { bar.low = v.get<double>(); hasLow=true; }
                break;
            case ColumnType::Close:
                if (v.is_number()) { bar.close = v.get<double>(); hasClose=true; }
                break;
            case ColumnType::Bid:
                if (v.is_number()) { bar.bid = v.get<double>(); hasBid=true; }
                break;
            case ColumnType::Ask:
                if (v.is_number()) { bar.ask = v.get<double>(); hasAsk=true; }
                break;
            case ColumnType::Volume:
                if (v.is_number_integer()) { bar.volume = v.get<long long>(); hasVol=true; }
                break;
            case ColumnType::Extra:
                if (v.is_number()) bar.extraColumns.push_back(v.get<double>());
                else if (v.is_string()) bar.extraColumns.push_back(v.get<std::string>());
                break;
            }
        }
        if (!hasVol) bar.volume = 0;
        // Fallback logic
        if (hasClose) {
            if (!hasBid) bar.bid = bar.close;
            if (!hasAsk) bar.ask = bar.close;
            if (!hasOpen) bar.open = bar.close;
            if (!hasHigh) bar.high = bar.close;
            if (!hasLow) bar.low = bar.close;
        } else if (hasBid && hasAsk) {
            double m = bar.midPrice(); if (!hasOpen) bar.open=m; if (!hasHigh) bar.high=m; if (!hasLow) bar.low=m; bar.close=m;
        } else if (hasBid) {
            if (!hasAsk) bar.ask=bar.bid; if (!hasOpen) bar.open=bar.bid; if (!hasHigh) bar.high=bar.bid; if (!hasLow) bar.low=bar.bid; bar.close=bar.bid;
        } else if (hasAsk) {
            if (!hasBid) bar.bid=bar.ask; if (!hasOpen) bar.open=bar.ask; if (!hasHigh) bar.high=bar.ask; if (!hasLow) bar.low=bar.ask; bar.close=bar.ask;
        } else {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}