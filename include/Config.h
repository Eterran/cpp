// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include "json.hpp"
#include "ColumnSpec.h"

class Config {
private:
    // Store parsed config directly in a json object
    nlohmann::json configData;
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

    std::vector<ColumnSpec> getColumnSpecs(const std::string& keyPath) const;

    // --- Setter ---
    template <typename T>
    void set(const std::string& key, const T& value); // Keep definition in .cpp

    bool has(const std::string& key) const;
};

#endif // CONFIG_H