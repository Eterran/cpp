#include "CSVParserStep.h"
#include "Utils.h"
#include "Bar.h"
#include <sstream>
#include <charconv>
#include <algorithm>

CSVParserStep::CSVParserStep(std::vector<ColumnSpec> specs_, const std::string& tsFormat_, char delimiter_)
    : specs(std::move(specs_)), tsFormat(tsFormat_), delimiter(delimiter_) {}

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
    // Track presence flags
    bool hasOpen=false, hasHigh=false, hasLow=false, hasClose=false, hasBid=false, hasAsk=false, hasVol=false;
    bar.extraColumns.clear();
    // Parse each spec
    for (auto& spec : specs) {
        if (spec.index < 0 || spec.index >= (int)fields.size()) continue;
        const auto& val = fields[spec.index];
        switch (spec.type) {
        case ColumnType::Timestamp:
            bar.timestamp = Utils::parseTimestamp(val, tsFormat);
            break;
        case ColumnType::Open: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.open=x; hasOpen=true; }
            break; }
        case ColumnType::High: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.high=x; hasHigh=true; }
            break; }
        case ColumnType::Low: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.low=x; hasLow=true; }
            break; }
        case ColumnType::Close: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.close=x; hasClose=true; }
            break; }
        case ColumnType::Bid: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.bid=x; hasBid=true; }
            break; }
        case ColumnType::Ask: {
            double x; auto res = std::from_chars(val.data(), val.data()+val.size(), x);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.ask=x; hasAsk=true; }
            break; }
        case ColumnType::Volume: {
            long long v; auto res = std::from_chars(val.data(), val.data()+val.size(), v);
            if (res.ec==std::errc() && res.ptr==val.data()+val.size()) { bar.volume=v; hasVol=true; }
            break; }
        case ColumnType::Extra:
            // try number, else string
            try {
                double x = std::stod(val);
                bar.extraColumns.push_back(x);
            } catch (...) {
                bar.extraColumns.push_back(val);
            }
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
        double m = bar.midPrice(); if (!hasOpen) bar.open = m; if (!hasHigh) bar.high = m; if (!hasLow) bar.low = m; bar.close = m;
    } else if (hasBid) {
        if (!hasAsk) bar.ask = bar.bid;
        if (!hasOpen) bar.open = bar.bid;
        if (!hasHigh) bar.high = bar.bid;
        if (!hasLow) bar.low = bar.bid;
        bar.close = bar.bid;
    } else if (hasAsk) {
        if (!hasBid) bar.bid = bar.ask;
        if (!hasOpen) bar.open = bar.ask;
        if (!hasHigh) bar.high = bar.ask;
        if (!hasLow) bar.low = bar.ask;
        bar.close = bar.ask;
    } else {
        return false;
    }
    return true;
}