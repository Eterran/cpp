#ifndef COLUMNSPEC_H
#define COLUMNSPEC_H

#include <string>

enum class ColumnType {
    Timestamp,
    Open,
    High,
    Low,
    Close,
    Bid,
    Ask,
    Volume,
    Extra
};

struct ColumnSpec {
    std::string name;
    ColumnType type;
    int index;
};

#endif // COLUMNSPEC_H