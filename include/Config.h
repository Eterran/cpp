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
    void set(const std::string& key, const T& value);

    bool has(const std::string& key) const;

    // --- Explicit Template Instantiations ---
    // template <typename T>
    // template std::string Config::get<std::string>(const std::string& key, const std::string& defaultValue) const;
    // template double Config::get<double>(const std::string& key, const double& defaultValue) const;
    // template int Config::get<int>(const std::string& key, const int& defaultValue) const;
    // template bool Config::get<bool>(const std::string& key, const bool& defaultValue) const;

    // template std::string Config::getNested<std::string>(const std::string& keyPath, const std::string& defaultValue) const;
    // template double Config::getNested<double>(const std::string& keyPath, const double& defaultValue) const;
    // template int Config::getNested<int>(const std::string& keyPath, const int& defaultValue) const;
    // template bool Config::getNested<bool>(const std::string& keyPath, const bool& defaultValue) const;

    // template void Config::set<std::string>(const std::string& key, const std::string& value);
    // template void Config::set<double>(const std::string& key, const double& value);
    // template void Config::set<int>(const std::string& key, const int& value);
    // template void Config::set<bool>(const std::string& key, const bool& value);
};

#endif // CONFIG_H