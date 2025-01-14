#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <cstring>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include "deeplx_translator.h"
#include "common/model_config.h"


namespace deeplx {

DeepLXTranslator::DeepLXTranslator(const common::ModelConfig& config) {

    // deeplx::DeepLXTranslator::Config
    deeplx::DeepLXTranslator::Config config_;

    if (!config.deeplx.enabled) {
        std::cout << "Translation is not enabled in config\n";
        return;
    }
    config_.url = config.deeplx.url;
    config_.token = config.deeplx.token;
    config_.target_lang = config.deeplx.target_lang;

    std::cout << "URL: " << config_.url << std::endl;
    std::regex url_regex("http://([^/:]+):?(\\d*)(/.*)?");
    std::smatch matches;
    if (!std::regex_match(config_.url, matches, url_regex)) {
        throw std::runtime_error("Invalid URL format"); 
    }
    std::cout << "Matches: " << matches.size() << std::endl;
    if (std::regex_match(config_.url, matches, url_regex)) {
        host_ = matches[1].str();
        std::cout << "Host: " << host_ << std::endl;
        port_ = matches[2].length() > 0 ? std::stoi(matches[2].str()) : 80;
        std::cout << "Port: " << port_ << std::endl;    
        path_ = matches[3].length() > 0 ? matches[3].str() : "/";
        std::cout << "Path: " << path_ << std::endl;
    } else {
        throw std::runtime_error("Invalid URL format");
    }
}

DeepLXTranslator::~DeepLXTranslator() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool DeepLXTranslator::needs_translation(const std::string& source_lang) const {
    std::string source_upper = source_lang;
    std::string target_upper = config_.target_lang;
    std::transform(source_upper.begin(), source_upper.end(), source_upper.begin(), ::toupper);
    std::transform(target_upper.begin(), target_upper.end(), target_upper.begin(), ::toupper);
    return source_upper != target_upper;
}

std::string DeepLXTranslator::make_http_request(const std::string& host, int port,
                                              const std::string& path, const std::string& body) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // Get host by name
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        close(sock);
        throw std::runtime_error("Failed to resolve host");
    }

    // Setup socket address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    // Connect
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        throw std::runtime_error("Failed to connect to server");
    }

    // Prepare HTTP request
    std::stringstream request;
    request << "POST " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Authorization: Bearer " << config_.token << "\r\n"
            << "Content-Length: " << body.length() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;

    std::string request_str = request.str();
    if (send(sock, request_str.c_str(), request_str.length(), 0) != static_cast<ssize_t>(request_str.length())) {
        close(sock);
        throw std::runtime_error("Failed to send request");
    }

    // Read response
    std::string response;
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
    }

    close(sock);
    return response;
}

DeepLXTranslator::HttpResponse DeepLXTranslator::send_post_request(const std::string& json_data) {
    std::string response = make_http_request(host_, port_, path_, json_data);
    
    // Parse HTTP response
    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("Invalid HTTP response");
    }

    // Get status code
    size_t status_line_end = response.find("\r\n");
    std::string status_line = response.substr(0, status_line_end);
    std::regex status_regex("HTTP/\\d\\.\\d (\\d+)");
    std::smatch status_match;
    int status_code = 500;
    if (std::regex_search(status_line, status_match, status_regex)) {
        status_code = std::stoi(status_match[1].str());
    }

    // Get body
    std::string body = response.substr(header_end + 4);

    return HttpResponse{status_code, body};
}

std::string DeepLXTranslator::translate(const std::string& text, const std::string& source_lang) {
    if (!needs_translation(source_lang)) {
        return text;
    }

    nlohmann::json requestJson = {
        {"text", text},
        {"source_lang", source_lang},
        {"target_lang", config_.target_lang}
    };
    std::string jsonStr = requestJson.dump();

    try {
        auto response = send_post_request(jsonStr);
        if (response.status_code != 200) {
            throw std::runtime_error("Server returned error status: " + 
                                   std::to_string(response.status_code));
        }

        auto responseJson = nlohmann::json::parse(response.body);
        
        // Check if the response code is successful
        if (responseJson["code"].get<int>() != 200) {
            throw std::runtime_error("Translation API returned error code: " + 
                                   std::to_string(responseJson["code"].get<int>()));
        }

        // Return the main translation result
        return responseJson["data"].get<std::string>();

        // Note: The response also contains:
        // - alternatives: array of alternative translations
        // - id: translation request ID
        // - method: translation method used
        // - source_lang: detected/used source language
        // - target_lang: target language
    } catch (const std::exception& e) {
        throw std::runtime_error("Translation failed: " + std::string(e.what()));
    }
}

} // namespace deeplx 