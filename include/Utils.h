#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <chrono>
#include <vector>

namespace Utils {
    // Parses "YYYYMMDD HHMMSSfff" format. Throws std::runtime_error on failure.
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr, std::string tsFmt);

    // Formats a time_point into a string (e.g., "YYYY-MM-DD HH:MM:SS.fff")
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    // Logs a message to the console with a timestamp.
    void logMessage(const std::string& message);

    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    
    std::string WideToUTF8(const std::wstring& wstr);
    std::wstring UTF8ToWide(const std::string& str);
}

#endif // UTILS_H