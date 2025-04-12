#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <chrono>
#include <vector> // Added in case a split function is needed later

namespace Utils {
    // Parses "YYYYMMDD HHMMSSfff" format. Throws std::runtime_error on failure.
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr, std::string tsFmt);

    // Formats a time_point into a string (e.g., "YYYY-MM-DD HH:MM:SS.fff")
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    // Logs a message to the console with a timestamp.
    void logMessage(const std::string& message);

    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    // Example of a potential future utility
    // std::vector<std::string> split(const std::string& s, char delimiter);

} // namespace Utils

#endif // UTILS_H