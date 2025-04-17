#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <string>

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual bool open() = 0;
    virtual bool getNext(std::string& record) = 0;
    virtual void close() = 0;
    // Optional: return total record count or -1 if unknown
    virtual long long count() const { return -1; }
};

#endif // DATASOURCE_H