#include "JSONParserStep.h"
#include "Utils.h"
#include "Bar.h"
#include "json.hpp"

JSONParserStep::JSONParserStep(const Config& config)
    : config(config) {}

bool JSONParserStep::parse(const std::string& record, Bar& bar) const {
    if (record.empty() || record.front() != '{') return false;
    try {
        auto obj = nlohmann::json::parse(record);
        auto getField = [&](const std::string& key, const std::string& def){
            return config.getNested<std::string>("/Data/" + key, def);
        };
        // parse timestamp
        std::string tsField = getField("API_Field_Timestamp", "timestamp");
        std::string tsFmt = config.getNested<std::string>("/Data/API_Timestamp_Format", "%Y-%m-%dT%H:%M:%S");
        bar.timestamp = Utils::parseTimestamp(obj.at(tsField).get<std::string>(), tsFmt);
        // parse numeric fields
        auto parseNum = [&](const std::string& key, double& out){ if (obj.contains(key) && obj[key].is_number()) { out = obj[key].get<double>(); return true;} return false; };
        auto parseInt = [&](const std::string& key, long long& out){ if (obj.contains(key) && obj[key].is_number_integer()) { out = obj[key].get<long long>(); return true;} return false; };
        bool hasOpen = parseNum(getField("API_Field_Open", "open"), bar.open);
        bool hasHigh = parseNum(getField("API_Field_High", "high"), bar.high);
        bool hasLow  = parseNum(getField("API_Field_Low", "low"), bar.low);
        bool hasClose= parseNum(getField("API_Field_Close", "close"), bar.close);
        bool hasBid  = parseNum(getField("API_Field_Bid", "bid"), bar.bid);
        bool hasAsk  = parseNum(getField("API_Field_Ask", "ask"), bar.ask);
        bool hasVol  = parseInt(getField("API_Field_Volume", "volume"), bar.volume);
        if (!hasVol) bar.volume = 0;
        // fallback logic
        if (hasClose) {
            if (!hasBid) bar.bid = bar.close;
            if (!hasAsk) bar.ask = bar.close;
            if (!hasOpen) bar.open = bar.close;
            if (!hasHigh) bar.high = bar.close;
            if (!hasLow) bar.low = bar.close;
        } else if (hasBid && hasAsk) {
            double mid = bar.midPrice(); if (!hasOpen) bar.open = mid; if (!hasHigh) bar.high = mid; if (!hasLow) bar.low = mid; bar.close = mid;
        } else if (hasBid) { if (!hasAsk) bar.ask = bar.bid; if (!hasOpen) bar.open = bar.bid; if (!hasHigh) bar.high = bar.bid; if (!hasLow) bar.low = bar.bid; bar.close = bar.bid; }
        else if (hasAsk) { if (!hasBid) bar.bid = bar.ask; if (!hasOpen) bar.open = bar.ask; if (!hasHigh) bar.high = bar.ask; if (!hasLow) bar.low = bar.ask; bar.close = bar.ask; }
        else { Utils::logMessage("JSONParserStep Warning: No price data"); return false; }
        // extra columns
        bar.extraColumns.clear();
        for (auto& [k,v] : obj.items()) {
            if (k == tsField) continue;
            if (k == getField("API_Field_Open","open") || k == getField("API_Field_High","high") ||
                k == getField("API_Field_Low","low") || k == getField("API_Field_Close","close") ||
                k == getField("API_Field_Bid","bid") || k == getField("API_Field_Ask","ask") ||
                k == getField("API_Field_Volume","volume")) continue;
            if (v.is_number()) bar.extraColumns.push_back(v.get<double>());
            else if (v.is_string()) bar.extraColumns.push_back(v.get<std::string>());
        }
        return true;
    } catch (const std::exception& e) {
        Utils::logMessage(std::string("JSONParserStep Error: ") + e.what());
        return false;
    }
}