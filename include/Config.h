// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>      // <--- ADD THIS INCLUDE for int64_t etc.
#include "json.hpp"     // <--- INCLUDE the full header HERE

// Remove the complex forward declaration and using alias lines below:
// namespace nlohmann { template<typename T, typename S, typename...> class basic_json; using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, nlohmann::adl_serializer, std::vector<std::uint8_t>>; }


class Config {
private:
    // Store parsed config directly in a json object
    nlohmann::json configData; // <--- Now this type is fully known
    std::string configFilePath;

    void setDefaultValues();

public:
    Config();

    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename = "") const;

    // --- Getters ---
    template <typename T>
    T get(const std::string& key, const T& defaultValue) const;

    template <typename T>
    T getNested(const std::string& keyPath, const T& defaultValue) const;

    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    // --- Setter ---
    template <typename T>
    void set(const std::string& key, const T& value); // Keep definition in .cpp

    bool has(const std::string& key) const;
};

// Template definitions for getters/setters should remain in Config.cpp
// Move explicit instantiations to Config.cpp if they were in the header.

#endif // CONFIG_H