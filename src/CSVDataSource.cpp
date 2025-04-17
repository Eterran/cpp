#include "CSVDataSource.h"
#include "Utils.h"
#include <iostream>

CSVDataSource::CSVDataSource(const std::string& filePath, char delimiter, bool skipHeader)
    : filePath(filePath), delimiter(delimiter), skipHeader(skipHeader)
{
}

CSVDataSource::~CSVDataSource() {
    close();
}

bool CSVDataSource::open() {
    fileStream.open(filePath);
    if (!fileStream.is_open()) {
        Utils::logMessage("CSVDataSource Error: Could not open file " + filePath);
        return false;
    }
    if (skipHeader) {
        std::string header;
        std::getline(fileStream, header);
    }
    return true;
}

bool CSVDataSource::getNext(std::string& record) {
    while (std::getline(fileStream, record)) {
        if (record.empty() || record[0] == '#') continue;
        return true;
    }
    return false;
}

void CSVDataSource::close() {
    if (fileStream.is_open()) fileStream.close();
}

long long CSVDataSource::count() const {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return -1;
    }
    long long cnt = 0;
    std::string line;
    if (skipHeader) {
        std::getline(in, line);
    }
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        ++cnt;
    }
    return cnt;
}