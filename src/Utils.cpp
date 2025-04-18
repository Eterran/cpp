#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip> 
#include <stdexcept>
#include <ctime> 
#include <vector>

namespace Utils {
    // Parses "YYYYMMDD HHMMSSfff" format. Throws std::runtime_error on failure.
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestampStr, std::string tsFmt) {
        // Expected format length check (basic sanity)
        if (timestampStr.length() < 15) { // "YYYYMMDD HHMMSS" is the minimum possible valid length
            throw std::runtime_error("Invalid timestamp format length: " + timestampStr);
        }

        std::tm tm = {};
        std::stringstream ss(timestampStr.substr(0, 15)); // Parse "YYYYMMDD HHMMSS" part

        // Note: MSVC might require different locale setup for get_time if system locale interferes
        // ss.imbue(std::locale::classic()); // Consider if parsing fails unexpectedly

        ss >> std::get_time(&tm, tsFmt.c_str()); // Use custom format string for parsing

        if (ss.fail()) {
            throw std::runtime_error("Failed to parse timestamp date/time part: " + timestampStr.substr(0, 15));
        }

        // Initialize milliseconds to 0 
        int milliseconds = 0;
        
        // Only try to parse milliseconds if there are more characters available
        if (timestampStr.length() > 15) {
            std::size_t ms_pos = 15; // Position after SS
            
            // Handle potential separator like '.' or just 'fff' directly after SS
            if (timestampStr.length() > ms_pos && !std::isdigit(timestampStr[ms_pos])) {
                ms_pos++; // Skip potential separator like '.' or space if format slightly differs
            }
            
            if (timestampStr.length() > ms_pos) {
                std::string ms_str = timestampStr.substr(ms_pos);
                try {
                    // Ensure we only parse digits and handle varying lengths (f, ff, fff)
                    std::string digits_only;
                    for(char c : ms_str) {
                        if (std::isdigit(c)) {
                            digits_only += c;
                        } else {
                            break; // Stop at first non-digit
                        }
                    }
                    if (!digits_only.empty()) {
                        // Pad with zeros if needed (e.g., "1" -> 100ms, "12" -> 120ms)
                        digits_only.resize(3, '0');
                        milliseconds = std::stoi(digits_only);
                    }
                } catch (const std::exception& e) {
                    // Ignore potential errors like stoi failing if ms part is weird, default to 0 ms
                    Utils::logMessage("Warning: Could not parse milliseconds from '" + ms_str + "': " + e.what());
                    milliseconds = 0;
                }
            }
        }

        // Convert std::tm to time_point (Note: timegm is non-standard but common on Linux/macOS, _mkgmtime on Windows)
        // Using C++20 chrono features would simplify this significantly. Pre-C++20 is more complex.
        #ifdef _WIN32
            // For Windows, use _mktime64 to avoid 32-bit time_t limitations
            std::time_t time_c = _mktime64(&tm);
        #else
            // For POSIX systems
            std::time_t time_c = mktime(&tm); // mktime assumes local time
            // If data is guaranteed UTC, use timegm:
            // std::time_t time_c = timegm(&tm);
        #endif

        if (time_c == -1) {
            throw std::runtime_error("Failed to convert std::tm to time_t for: " + timestampStr);
        }

        auto time_point = std::chrono::system_clock::from_time_t(time_c);
        time_point += std::chrono::milliseconds(milliseconds);

        return time_point;
    }

    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto now_c = std::chrono::system_clock::to_time_t(tp);
        std::tm now_tm = {};
    #ifdef _WIN32
        localtime_s(&now_tm, &now_c); // Windows specific thread-safe version
    #else
        localtime_r(&now_c, &now_tm); // POSIX specific thread-safe version
    #endif

        // Get milliseconds
        auto duration = tp.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;

        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &now_tm);

        std::stringstream ss;
        ss << buffer << "." << std::setfill('0') << std::setw(3) << millis.count();
        return ss.str();
    }

    // Logs a message to the console with a timestamp.
    void logMessage(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        // Using std::put_time for thread-safe formatting if available (C++11)
        // Fallback to std::localtime which might not be thread-safe without external locking
        std::tm now_tm = {};
    #ifdef _WIN32
        localtime_s(&now_tm, &now_c); // Windows specific thread-safe version
    #else
        localtime_r(&now_c, &now_tm); // POSIX specific thread-safe version
        // If neither is available, fall back to potentially non-thread-safe localtime
        // if (!localtime_r(&now_c, &now_tm)) {
        //     std::tm* tm_ptr = std::localtime(&now_c);
        //     if (tm_ptr) now_tm = *tm_ptr;
        // }
    #endif

        // Get milliseconds
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;

        char buffer[80];
        // Format time manually if put_time causes issues or isn't available easily
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &now_tm);

        std::cout << "[" << buffer << "." << std::setfill('0') << std::setw(3) << millis.count() << "] " << message << std::endl;
    }

    std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
        std::time_t timeT = std::chrono::system_clock::to_time_t(tp);
        std::tm tm;
        localtime_s(&tm, &timeT); // thread safe?
    
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    #ifdef _WIN32
    #include <Windows.h>

    std::string WideToUTF8(const std::wstring& wstr) {
        if (wstr.empty()) return {};

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
                                            nullptr, 0, nullptr, nullptr);
        std::string str(size_needed - 1, 0);  // exclude null terminator
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1,
                            &str[0], size_needed, nullptr, nullptr);
        return str;
    }

    std::wstring UTF8ToWide(const std::string& str) {
        if (str.empty()) return {};
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring wstr(size_needed - 1, 0);  // exclude null terminator
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
        return wstr;
    }
    #endif
}