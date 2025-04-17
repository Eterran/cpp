#ifndef CSVDATASOURCE_H
#define CSVDATASOURCE_H

#include "DataSource.h"
#include <string>
#include <fstream>

class CSVDataSource : public IDataSource {
public:
    CSVDataSource(const std::string& filePath, char delimiter, bool skipHeader);
    ~CSVDataSource() override;

    bool open() override;
    bool getNext(std::string& record) override;
    void close() override;
    long long count() const override;

private:
    std::string filePath;
    char delimiter;
    bool skipHeader;
    mutable std::ifstream fileStream;  // mutable to allow count() const
};

#endif // CSVDATASOURCE_H