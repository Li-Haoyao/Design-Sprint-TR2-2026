#pragma once

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

struct HttpResponse {
    long status = 0;
    std::string body;
};

inline json read_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open " + path);
    json value;
    in >> value;
    return value;
}

inline void write_json_file(const std::string& path, const json& value) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write " + path);
    out << std::setw(2) << value << "\n";
}

inline std::string lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return text;
}

inline std::string iso_utc_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &raw);
#else
    gmtime_r(&raw, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

inline size_t curl_write(void* ptr, size_t size, size_t nmemb, void* userdata) {
    const size_t total = size * nmemb;
    static_cast<std::string*>(userdata)->append(static_cast<char*>(ptr), total);
    return total;
}

inline HttpResponse post_json(
    const std::string& url,
    const json& body,
    const std::vector<std::string>& extra_headers,
    long timeout_seconds = 90
) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Could not initialise libcurl");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    for (const auto& h : extra_headers) {
        headers = curl_slist_append(headers, h.c_str());
    }

    const std::string payload = body.dump();
    HttpResponse result;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CareMate-AI-Design-Sprint/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(code));
    }
    return result;
}

inline bool contains_any(const std::string& text, const std::vector<std::string>& terms) {
    const std::string lowered = lower_copy(text);
    for (const auto& term : terms) {
        if (lowered.find(lower_copy(term)) != std::string::npos) return true;
    }
    return false;
}

inline std::string json_error_message(const json& response) {
    try {
        if (response.contains("error")) {
            if (response["error"].is_object() && response["error"].contains("message"))
                return response["error"]["message"].get<std::string>();
            if (response["error"].is_string())
                return response["error"].get<std::string>();
        }
    } catch (...) {}
    return "";
}
