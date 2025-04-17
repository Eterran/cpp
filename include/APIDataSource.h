#ifndef APIDATASOURCE_H
#define APIDATASOURCE_H

#include "DataSource.h"
#include "Config.h"
#include <string>
#include <vector>

class APIDataSource : public IDataSource {
public:
    APIDataSource(const std::string& url, const Config& config);
    ~APIDataSource() override;

    bool open() override;
    bool getNext(std::string& record) override;
    void close() override;
    long long count() const override;

private:
    std::string url;
    const Config& config;
    std::vector<std::string> records;
    size_t current;
    bool fetched;
};

#endif // APIDATASOURCE_H