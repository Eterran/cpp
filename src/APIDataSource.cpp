#include "APIDataSource.h"
#include "Utils.h"
#include <curl/curl.h>
#include "json.hpp"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

APIDataSource::APIDataSource(const std::string& url, const Config& config)
    : url(url), config(config), current(0), fetched(false) {}

APIDataSource::~APIDataSource() {
    close();
}

bool APIDataSource::open() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        Utils::logMessage("APIDataSource Error: Failed to init CURL");
        return false;
    }
    std::string buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    // Optional: set timeout and headers from config
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        Utils::logMessage(std::string("APIDataSource Error: CURL request failed: ") + curl_easy_strerror(res));
        return false;
    }
    try {
        auto jsonArr = nlohmann::json::parse(buffer);
        if (!jsonArr.is_array()) {
            Utils::logMessage("APIDataSource Error: JSON response is not an array");
            return false;
        }
        records.clear();
        for (auto& rec : jsonArr) {
            records.push_back(rec.dump());
        }
        fetched = true;
        current = 0;
        return true;
    } catch (const std::exception& e) {
        Utils::logMessage(std::string("APIDataSource Error: JSON parse error: ") + e.what());
        return false;
    }
}

bool APIDataSource::getNext(std::string& record) {
    if (!fetched || current >= records.size()) return false;
    record = records[current++];
    return true;
}

void APIDataSource::close() {
    records.clear();
    current = 0;
    fetched = false;
}

long long APIDataSource::count() const {
    return fetched ? static_cast<long long>(records.size()) : -1;
}