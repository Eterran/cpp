#include "CSVParserStep.h"
#include "Utils.h"
#include "Bar.h"
#include <sstream>
#include <charconv>
#include <algorithm>

CSVParserStep::CSVParserStep(const Config& config)
    : config(config) {}

bool CSVParserStep::parse(const std::string& record, Bar& bar) const {
    if (record.empty() || record.front() == '{') return false;
    // Load delimiter and column indices from config
    char delim = config.getNested<std::string>("/Data/CSV_Delimiter", ",")[0];
    int tsCol = config.getNested<int>("/Data/CSV_Timestamp_Col", 0);
    std::string tsFmt = config.getNested<std::string>("/Data/CSV_Timestamp_Format", "%Y%m%d %H%M%S");
    int openCol = config.getNested<int>("/Data/CSV_Open_Col", -1);
    int highCol = config.getNested<int>("/Data/CSV_High_Col", -1);
    int lowCol = config.getNested<int>("/Data/CSV_Low_Col", -1);
    int closeCol = config.getNested<int>("/Data/CSV_Close_Col", -1);
    int bidCol = config.getNested<int>("/Data/CSV_Bid_Col", -1);
    int askCol = config.getNested<int>("/Data/CSV_Ask_Col", -1);
    int volCol = config.getNested<int>("/Data/CSV_Volume_Col", -1);

    // Split fields
    std::stringstream ss(record);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(ss, field, delim)) {
        field.erase(0, field.find_first_not_of(" \t\n\r\f\v"));
        field.erase(field.find_last_not_of(" \t\n\r\f\v") + 1);
        fields.push_back(field);
    }
    try {
        // Timestamp
        if (tsCol < 0 || tsCol >= (int)fields.size()) return false;
        bar.timestamp = Utils::parseTimestamp(fields[tsCol], tsFmt);
        // Parsing helpers
        auto parseD = [&](int idx, double& out) {
            if (idx < 0 || idx >= (int)fields.size()) return false;
            auto& s = fields[idx]; auto res = std::from_chars(s.data(), s.data()+s.size(), out);
            return res.ec==std::errc() && res.ptr==s.data()+s.size();
        };
        auto parseL = [&](int idx, long long& out) {
            if (idx < 0 || idx >= (int)fields.size()) return false;
            auto& s = fields[idx]; auto res = std::from_chars(s.data(), s.data()+s.size(), out);
            return res.ec==std::errc() && res.ptr==s.data()+s.size();
        };
        bool hO = parseD(openCol, bar.open);
        bool hH = parseD(highCol, bar.high);
        bool hL = parseD(lowCol, bar.low);
        bool hC = parseD(closeCol, bar.close);
        bool hB = parseD(bidCol, bar.bid);
        bool hA = parseD(askCol, bar.ask);
        bool hV = parseL(volCol, bar.volume);
        if (!hV) bar.volume = 0;
        // Fallbacks
        if (hC) {
            if (!hB) bar.bid = bar.close;
            if (!hA) bar.ask = bar.close;
            if (!hO) bar.open = bar.close;
            if (!hH) bar.high = bar.close;
            if (!hL) bar.low = bar.close;
        } else if (hB && hA) {
            double m = bar.midPrice(); if (!hO) bar.open=m; if(!hH) bar.high=m; if(!hL) bar.low=m; bar.close=m;
        } else if (hB) { if(!hA) bar.ask=bar.bid; if(!hO) bar.open=bar.bid; if(!hH) bar.high=bar.bid; if(!hL) bar.low=bar.bid; bar.close=bar.bid; }
        else if (hA) { if(!hB) bar.bid=bar.ask; if(!hO) bar.open=bar.ask; if(!hH) bar.high=bar.ask; if(!hL) bar.low=bar.ask; bar.close=bar.ask; }
        else return false;
        // Extra columns
        bar.extraColumns.clear();
        std::vector<int> used={tsCol,openCol,highCol,lowCol,closeCol,bidCol,askCol,volCol};
        for (int i=0;i<(int)fields.size();++i) {
            if (std::find(used.begin(),used.end(),i)!=used.end()) continue;
            double dv; auto pr=std::from_chars(fields[i].data(),fields[i].data()+fields[i].size(),dv);
            if (pr.ec==std::errc()&&pr.ptr==fields[i].data()+fields[i].size()) bar.extraColumns.push_back(dv);
            else bar.extraColumns.push_back(fields[i]);
        }
        return true;
    } catch(...) {
        return false;
    }
}